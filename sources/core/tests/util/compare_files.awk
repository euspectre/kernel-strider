#!/bin/awk -f

# For each line of input...
{
	if (tolower($1) != tolower($2)) {
		printf("Mismatch: expected \"%s\", found \"%s\"\n", $1, $2)
		exit 1
	}
}
