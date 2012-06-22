/* Scopes hierarchy for CTFReader */
#ifndef CTF_SCOPE_H_INCLUDED
#define CTF_SCOPE_H_INCLUDED

#include <kedr/ctf_reader/ctf_reader.h>

#include <numeric> /* accumulate algorithm */

#include <kedr/ctf_reader/ctf_hash.h>

class HashableString : public std::string
{
public:
	HashableString(const std::string& str) : std::string(str) {}
	HashableString(const char* str) : std::string(str) {}

    struct HashOp : public std::binary_function<int, char, int>
    {
        int operator()(int hash_partial, char c) {return hash_partial * 101 + c;}
    };
    
    int hash(void) const {return std::accumulate(begin(), end(), 0, HashOp());}
};

class CTFScope
{
public:
	/* Create scope without parent */
	CTFScope(void);
	
	virtual ~CTFScope(void);
	/* Added type will be deleted automatically */
	void addType(CTFType* type);
    /* Added scope will be deleted automatically */
    void addScope(CTFScope* scope);
    
    /* Assign name to the given type */
    void addTypeName(const CTFType* type, const std::string& name);
    /* Assign name to the given struct type. */
    void addStructName(const CTFTypeStruct* typeStruct,
		const std::string& name);
	/* Assign name to the given enum type. */
    void addEnumName(const CTFTypeEnum* typeEnum,
		const std::string& name);
	/* Assign name to the given variant type. */
    void addVariantName(const CTFTypeVariant* typeVariant,
		const std::string& name);
    
    /*
	 * Search type with given name in scope and all of its parents.
	 */
	const CTFType* findType(const std::string& name) const;
	const CTFTypeStruct* findStruct(const std::string& name) const;
	const CTFTypeEnum* findEnum(const std::string& name) const;
	const CTFTypeVariant* findVariant(const std::string& name) const;

	/*
	 * Search type with given name only in given scope.
	 */
	const CTFType* findTypeStrict(const std::string& name) const;
	const CTFTypeStruct* findStructStrict(const std::string& name) const;
	const CTFTypeEnum* findEnumStrict(const std::string& name) const;
	const CTFTypeVariant* findVariantStrict(const std::string& name) const;


private:
	CTFScope(const CTFScope& scope);/* Not implemented */

	/* Parent scope (NULL for scope just created)*/
	const CTFScope* parent;
    /* Types */
    std::vector<CTFType*> types;
    /* Scopes */
    std::vector<CTFScope*> scopes;
    /* Hash table for search types by name */
    typedef HashTable<HashableString, const CTFType*> typeNames_t;
    typeNames_t typeNames;
    
    typedef HashTable<HashableString, const CTFTypeStruct*> structNames_t;
    structNames_t structNames;
    
    typedef HashTable<HashableString, const CTFTypeEnum*> enumNames_t;
    enumNames_t enumNames;
    
    typedef HashTable<HashableString, const CTFTypeVariant*> variantNames_t;
    variantNames_t variantNames;
};

class CTFScopeTop : public CTFScope
{
public:
	void addParameter(const std::string& name, const std::string& value);
	
	const std::string* findParameter(const std::string&) const;
private:
	/* Hash table with parameters */
	typedef HashTable<HashableString, std::string> parameters_t;
	parameters_t parameters;
};

class CTFScopeRoot : public CTFScope
{
public:
    void addTopScopeName(const CTFScopeTop* scopeTop, const std::string& name);
    
    const std::string* findParameter(const std::string& name) const;
private:
	typedef HashTable<HashableString, const CTFScopeTop*> scopesTop_t;
	scopesTop_t scopesTop;
};


#endif /* CTF_SCOPE_H_INCLUDED */