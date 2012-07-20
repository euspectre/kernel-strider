#include "text_streamed_converter.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

static int write_convertion(const char* text, size_t size, void* data)
{
	FILE* out = (FILE*)data;
	int result = fwrite(text, size, 1, out);
	if(result != 1)
	{
		perror("Failed to output result of address conversion.");
		return -1;
	}
	return 0;
}


static int
process_report(FILE* report, FILE* out, 
	struct text_streamed_converter* converter)
{
	// Process the report line by line. The lines that match the 
	// expression "^\s*#\d+\s+<hex_address>:.*$" should be processed,
	// the remaining ones should be output unchanged.
	
    char* line = NULL;
    size_t n_bytes = 0;
    
    ssize_t len;
    
    int result = 0;
	
    while((len = getline(&line, &n_bytes, report)) != -1)
    {
        const char* current = line;
        /* Skip spaces at the start of the string */
        while(isspace(*current)) current++;
        
        /* Check and skip # */
        if(*current != '#') goto unchanged;
        current++;
        /* Check and skip digits */
        if(!isdigit(*current)) goto unchanged;
        current++;
        
        while(isdigit(*current)) current++;
        
        /* Skip spaces after #<number> construction */
        while(isspace(*current)) current++;
        
        /* Check hexadecimal number(address) */
        if(!isxdigit(*current)) goto unchanged;
        
        const char* number_start = current;
        const char* number_end = current;
		
        while(isxdigit(*number_end) || (*number_end == 'x') || (*number_end == 'X'))
        {
			number_end++;
        }
        
		/* Output everything before address */
		result = fwrite(line, current - line, 1, out);
		if(result != 1)
		{
			fprintf(stderr, "Error occures while write into output file.");
			result = -EINVAL;
			break;
		}
		result = 0;

		// On x86-64, TSan may use the higher 6 bits of the address
		// for its own data and it does not restore these bits when
		// outputting the address, just zeroes them (OK for user-
		// space applications but not for the kernel). A workaround 
		// is provided here if an address in the report begins with
		// "0x3ff", this part of the address is replaced with 
		// "0xffff".
		if((strncmp(number_start, "0x3ff", 5) == 0)
			|| (strncmp(number_start, "0x3FF", 5) == 0))
		{
			result = text_streamed_converter_put_text(converter, "0xffff", 6);
			if(result) break;
			result = text_streamed_converter_put_text(converter,
				number_start + 5, number_end - number_start - 5);
			if(result) break;
		}
		else
		{
			result = text_streamed_converter_put_text(converter,
				number_start, number_end - number_start);
			if(result) break;
		}

		result = text_streamed_converter_convert(converter);
		if(result) break;
		
		/* Conversion is 1:1*/
		result = text_streamed_converter_get_text(converter,
			write_convertion, out);
		if(result) break;

		/* Output everything after address */
		result = fwrite(number_end, line + len - number_end, 1, out);
		if(result != 1)
		{
			fprintf(stderr, "Error occures while write into output file.");
			result = -EINVAL;
			break;
		}
		result = 0;
		continue;

unchanged:
		result = fwrite(line, len, 1, out);
		if(result != 1)
		{
			fprintf(stderr, "Error occures while write into output file.");
			result = -EINVAL;
			break;
		}
		result = 0;
	}
	
	free(line);
	
	return result;
}

int main(int argc, char** argv)
{
	if(argc == 1)
	{
		fprintf(stderr, "Usage: tsan_report_processor "
			"<converter-program> <converter-program-args...>\n");
		return 1;
	}
	
	struct text_streamed_converter converter;
	
	int result = text_streamed_converter_start(&converter, argv[1], argv + 1);
	if(result) return result;
	
	result = process_report(stdin, stdout, &converter);
	
	text_streamed_converter_stop(&converter);
	
	return result;
}