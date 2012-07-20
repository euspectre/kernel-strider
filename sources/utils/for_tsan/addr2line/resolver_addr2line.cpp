/* Translate address in memory into string in format
 * 
 *          <function> <file>: <line>
 */

#include <gelf.h>
#include <iostream>
#include <fstream>

#include <fcntl.h>

#include <stdint.h>
#include <errno.h>
#include <cstring>

#include <string>
#include <vector>
#include <set>
#include <map>

#include <stdexcept>

#include <iomanip>

#include "text_streamed_converter.h"

using std::string;
using std::vector;
using std::map;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;

/* Base object for different translators */
class AddressTranslator
{
public:
    virtual ~AddressTranslator(){};
    /* 
     * Write result of translation into stdout, without newline.
     * 
     * Return 0 on success, negative error code otherwise.
     */
    virtual int translate(uint64_t addr) = 0;
};

struct SectionRecord
{
    string name;
    uint64_t addr;
};

static vector<SectionRecord> loadSectionRecords(const char* filename);

template<class T>
struct Range
{
    T start;
    T end;
    
    bool operator<(const struct Range<T>& range) const {return end <= range.start;}
    
    Range(T start, T end): start(start), end(end) {}
    Range(T value): start(value), end(value + 1) {}
};

/* 
 * Wrapper around addr2line utility as text_streamed_converter
 * for automatically destroying.
 */
struct Addr2Line
{
    struct text_streamed_converter converter;
    
    Addr2Line(const char* module, const char* sectionName);
    ~Addr2Line();
};


class Addr2lineTranslator: public AddressTranslator
{
public:
    Addr2lineTranslator(const char* sectionsFile, const char* moduleFile);
    
    int translate(uint64_t addr);
private:
    const string moduleFile;
    struct Section
    {
        const char* module;
        const string name;
        /* Lazy initialization */
        mutable Addr2Line* addr2line;
        
        Section(const char* module, const string& name):
            module(module), name(name), addr2line(NULL) {}
        ~Section(void) {delete addr2line;}
        
        struct text_streamed_converter* getConverter(void) const
        {
            if(!addr2line) addr2line = new Addr2Line(module, name.c_str());
            return &addr2line->converter;
        }
    };
    
    map<Range<uint64_t>, Section> sections;
    
    void setupSections(const vector<SectionRecord>& records);
};


int main(int argc, char** argv)
{
    if(argc < 3)
    {
        cerr << "Usage: addr_to_symbol <sections-file> <module-file>\n";
        return EINVAL;
    }
    
    Addr2lineTranslator translator(argv[1], argv[2]);
    
    cin >> std::hex >> std::showbase;
    
    while(cin && cin.peek() != std::istream::traits_type::eof())
    {
        /* read address */
        uint64_t addr;
        cin >> addr;
        if(!cin)
        {
            cerr << "Failed to parse hexadecimal integer";
            return EINVAL;
        }
        
        if(translator.translate(addr)) break;
        printf("\n");
        fflush(stdout);
        
        /* skip newline */
        if(cin.peek() != std::istream::traits_type::eof())
        {
            char c = cin.get();
            if(c != '\n')
            {
                cerr << "Exceeded characters in string: '" << c << "'\n";
                return EINVAL;
            }
        }
    }
    return 0;
}

/*********************Implementation **********************************/
Addr2Line::Addr2Line(const char* module, const char* section_name)
{
    const char* addr2lineParams[] = {"addr2line", "-s", "-e", module,
        "-j", section_name, "-f", NULL};
    
    int result = text_streamed_converter_start(&converter,
        "addr2line", (char**)addr2lineParams);
    
    if(result) throw std::runtime_error("Failed to start addr2line");
}

Addr2Line::~Addr2Line(void)
{
    text_streamed_converter_stop(&converter);
}

// Whitespace characters
extern const string whitespaceList = " \t\n\r\v\a\b\f";

static std::istream& skipws(std::istream& is)
{
    while(is)
    {
        char c = is.peek();
        //cerr << "Character code is " << (int)c << "" << endl;
        if(c == std::istream::traits_type::eof()
            || whitespaceList.find(is.peek()) == whitespaceList.npos) break;
        //cerr << "Skip character '" << c << "'" << endl;
        is.get();
    }
    return is;
}

vector<SectionRecord> loadSectionRecords(const char* filename)
{
    // Load the section information from the given file, record by record.
    std::ifstream is(filename);
    if (!is) {
        throw std::runtime_error("Failed to open file with section adresses.");
    }
    
    int line = 0;
    vector<SectionRecord> result;
    
    is >> std::hex >> std::showbase;
    while(1)
    {
        if(!skipws(is))
        {
            throw std::runtime_error("Error while read file with sections.");
        }
        if(is.eof()) break;
        
        uint64_t addr;
        is >> addr;
        if(!is)
        {
            cerr << "Error at line " << line + 1 << endl;
            throw std::runtime_error("Failed to parse address as integer.");
        }
        
        if(!skipws(is))
        {
            throw std::runtime_error("Error while read file with sections.");
        }
        
        char sectionName[100];
        is >> std::setw(sizeof(sectionName)) >> sectionName;
        if(!is)
        {
            throw std::runtime_error("No section name.");
        }
        
        SectionRecord record;
        record.name = sectionName;
        record.addr = addr;
        
        result.push_back(record);
        line++;
    }
    return result;
}

Addr2lineTranslator::Addr2lineTranslator(const char* sectionsFile,
    const char* moduleFile) : moduleFile(moduleFile)
{
    vector<SectionRecord> records = loadSectionRecords(sectionsFile);
    
    setupSections(records);
}

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

int Addr2lineTranslator::translate(uint64_t addr)
{
    map<Range<uint64_t>, Section>::const_iterator iter =
        sections.find(Range<uint64_t>(addr));
        
    if(iter != sections.end())
    {
        const Section& section = iter->second;
        int offset = addr - iter->first.start;
        
        struct text_streamed_converter* converter = section.getConverter();
        
        char offsetStr[20];
        int n_bytes = snprintf(offsetStr, sizeof(offsetStr), "0x%x",
            (unsigned int)offset);
        if(text_streamed_converter_put_text(converter, offsetStr, n_bytes))
            return -EINVAL;

        if(text_streamed_converter_convert(converter))
            return -EINVAL;
        
        /* Extract function name */
        if(text_streamed_converter_get_text(converter, write_convertion,
            stdout))
            return -EINVAL;
        printf(" ");
        /* Extract source file plus line */
        if(text_streamed_converter_get_text(converter, write_convertion,
            stdout))
            return -EINVAL;
        
        //printf(" (0x%llx)", (long long)addr);
    }
    else
    {
        /* When cannot determine section output adress without change. */
        printf("0x%llx", (long long)addr);
    }
    return 0;
}

void Addr2lineTranslator::setupSections(
    const vector<SectionRecord>& records)
{
    if(elf_version(EV_CURRENT) == EV_NONE)
    {
        throw std::runtime_error("Libelf initialization failed");
    }
    
    int fd = open(moduleFile.c_str(), O_RDONLY);
    if(fd == -1)
    {
        cerr << "Failed to open file '" << moduleFile << "': "
            << strerror(errno) << "." << endl;
        throw std::runtime_error("Failed to open module file.");
    }
    
    Elf* e = elf_begin(fd, ELF_C_READ, NULL);
    if(e == NULL)
    {
        close(fd);
        throw std::runtime_error("elf_begin() failed");
    }
    
    try{
        if(elf_kind(e) != ELF_K_ELF)
        {
            throw std::runtime_error("Not an ELF");
        }
        
        /* Extract section sizes and fill 'sections' map. */
        size_t shdrstrndx;
        elf_getshdrstrndx(e, &shdrstrndx);
        
        for(Elf_Scn* scn = elf_nextscn(e, NULL); scn; scn = elf_nextscn(e, scn))
        {
            GElf_Shdr shdr;
            gelf_getshdr(scn, &shdr);
            
            const char* scn_name = elf_strptr(e, shdrstrndx, shdr.sh_name);
            if((scn_name == NULL) || (*scn_name == '\0')) continue;
            
            for(int i = 0; i < (int)records.size(); i++)
            {
                const SectionRecord& record = records[i];
                if(record.name == scn_name)
                {
                    sections.insert(std::make_pair(
                        Range<uint64_t>(record.addr, record.addr + shdr.sh_size),
                        Section(moduleFile.c_str(), record.name)));
                    break;
                }
            }
        }
    }
    catch(std::exception&)
    {
        elf_end(e);
        close(fd);
        
        throw;
    }
    elf_end(e);
    close(fd);
}


