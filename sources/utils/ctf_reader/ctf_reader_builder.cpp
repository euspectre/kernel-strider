#include <kedr/ctf_reader/ctf_reader.h>
#include "ctf_scope.h"
#include "ctf_ast.h"

#include "ctf_root_type.h"

#include <sstream> /* extract integers from string */
#include <iostream> /* cerr */

#include <algorithm>

#include "ctf_reader_parser.h"

#include <stdexcept> /* logic_error and other exceptions */

/* Parse given string as 16-byte UUID*/
//static void parseUUID(const std::string& uuidStr, unsigned char uuid[16]);

class CTFReaderBuilder
{
public:
    CTFReaderBuilder(CTFReader& reader);
    
    void build(const CTFAST& ast);
private:
    /* 
     * Return native order of bytes.
     * 
     * NOTE: if corresponded parameter in metadata is not set,
     * throw exception. So, call this function only when native order is
     * really needed.
     */
    CTFTypeInt::ByteOrder getNativeByteOrder(void) const;
    
    /* Build integer type according to its scope */
    class TypeIntBuilder : public CTFASTStatement::Visitor
    {
    public:
        TypeIntBuilder(CTFReaderBuilder* builder) : builder(builder) {}
        
        /* 
         * Main function.
         * 
         * Set parameters for integer type. Fix parameters at the end.
         */
        void build(const CTFASTScopeInt& scopeInt, CTFTypeInt* typeInt);
    
        void visitParameterDef(const CTFASTParameterDef& parameterDef);
    private:
        CTFReaderBuilder* builder;
        CTFTypeInt* typeInt;
        /* 
         * Whether byte order parameter was set for integer.
         * 
         * If this parameter is not set at the end of scope,
         * byte order corresponded for 'native' is set.
         */
        bool byteOrderIsSet;
    };
    /* Build enumeration type according to its scope. */
    class TypeEnumBuilder: public CTFASTEnumValueDecl::Visitor
    {
    public:
        TypeEnumBuilder(void) {};
        
        /* 
         * Main function.
         * 
         * Set named values for enum type.
         */
        void build(const CTFASTScopeEnum& scopeEnum,
            CTFTypeEnum* typeEnum);
        
        void visitSimple(const CTFASTEnumValueDeclSimple& enumValueDeclSimple);
        void visitPresize(const CTFASTEnumValueDeclPresize& enumValueDeclPresize);
        void visitRange(const CTFASTEnumValueDeclRange& enumValueDeclRange);
    private:
        CTFTypeEnum* typeEnum;
        //TODO: process large unsigned enum values
        int64_t nextValue;
    };
    
    /* 
     * Build type according to sequence of type modificators.
     */
    class TypePostModsBuilder: public CTFASTTypePostMod::Visitor
    {
    public:
        /*
         * 'scope' - current scope.
         * 'typeConnected' - structure or variant type, which is connected
         * to the scope. This type is used for resolving tags.
         * 
         * 'typeConnected' may be NULL. In that case only root type
         * is used for resolving tags.
         */
        TypePostModsBuilder(CTFReaderBuilder* builder,
            CTFScope* scope, CTFType* typeConnected)
            : builder(builder), scope(scope), typeConnected(typeConnected) {}
        
        /*
         * Main function.
         * 
         * Construct type according to modifiers to initial type.
         */
        const CTFType* build(const CTFASTTypePostMods& typePostMods,
            const CTFType* typeInitial);
        
        void visitArray(const CTFASTArrayMod& arrayMod);
        void visitSequence(const CTFASTSequenceMod& sequenceMod);
    private:
        CTFReaderBuilder* builder;
        CTFScope* scope;
        CTFType* typeConnected;
        
        const CTFType* typeCurrent;
    };
    /* 
     * Base builder of scopes, which allow to declare types
     * (really, all CTFScope classes).
     */
    class ScopeBuilder : public CTFASTStatement::Visitor
    {
    public:
        ScopeBuilder(CTFReaderBuilder* builder) : builder(builder) {}
        
        /* 
         * Build given scope.
         * 
         * 'typeConnected' - structure or variant type, which is connected
         * to the scope. This type is used for resolving tags.
         * 
         * 'typeConnected' may be NULL. In that case only root type
         * is used for resolving tags.
         */
        void build(const CTFASTScope& scope,
            CTFScope* scopeBuilded, CTFType* typeConnected);
        
        void visitStructDecl(const CTFASTStructDecl& structDecl);
        void visitVariantDecl(const CTFASTVariantDecl& variantDecl);
        void visitEnumDecl(const CTFASTEnumDecl& enumDecl);
        void visitTypedefDecl(const CTFASTTypedefDecl& typedefDecl);
    protected:
        CTFReaderBuilder* builder;
        CTFScope* scope;
        CTFType* typeConnected;
    };
    
    /* Build structure fields according to structure scope. */
    class TypeStructBuilder : public ScopeBuilder
    {
    public:
        TypeStructBuilder(CTFReaderBuilder* builder)
            : ScopeBuilder(builder) {}
        
        /* 
         * Main function.
         * 
         * Set fields for given structure.
         * 
         * 'scope' - scope of that structure.
         */
        void build(const CTFASTScopeStruct& scopeStruct,
            CTFTypeStruct* typeStruct, CTFScope* scope);
    public:
        void visitFieldDecl(const CTFASTFieldDecl& fieldDecl);
    private:
        CTFTypeStruct* typeStruct;
    };

    /* Build variant fields according to variant scope. */
    class TypeVariantBuilder : public ScopeBuilder
    {
    public:
        TypeVariantBuilder(CTFReaderBuilder* builder)
            : ScopeBuilder(builder) {}
        
        /* 
         * Main function.
         * 
         * Set fields for given variant.
         * 
         * 'scope' - scope of that variant.
         */
        void build(const CTFASTScopeVariant& scopeVariant,
            CTFTypeVariant* typeVariant, CTFScope* scope);
    public:
        void visitFieldDecl(const CTFASTFieldDecl& fieldDecl);
    private:
        CTFTypeVariant* typeVariant;
    };
    
    /* 
     * Builder of top(named) scope.
     * 
     * Assign types corresponded for this scope.
     */
    class TopScopeBuilder: public ScopeBuilder
    {
    public:
        TopScopeBuilder(CTFReaderBuilder* builder)
            : ScopeBuilder(builder) {}
        
        /* 
         * Main function.
         * 
         * Build given scope with given name.
         */
        void build(const CTFASTScopeTop& scopeTop,
            CTFScopeTop* scope, const std::string& name);
        
        void visitParameterDef(const CTFASTParameterDef& parameterDef);
        void visitTypeAssignment(const CTFASTTypeAssignment& typeAssignment);
    private:
        CTFScopeTop* scope;
        const std::string* name;
    };
    
    /* Builder of root scope. */
    class RootScopeBuilder: public ScopeBuilder
    {
    public:
        RootScopeBuilder(CTFReaderBuilder* builder)
            : ScopeBuilder(builder) {}
        
        /* 
         * Main function.
         * 
         * Build root scope of the reader, for which builder is created.
         */
        void build(const CTFASTScopeRoot& scopeRoot);
        
        void visitTopScopeDecl(const CTFASTTopScopeDecl& topScopeDecl);
    };

    /*
     * Builder of type according to its specification.
     */
    class TypeSpecBuilder: public CTFASTTypeSpec::Visitor
    {
    public:
        /*
         * 'scope' - current scope(used for searching types by name)
         * 'typeConnected' - type connected to the current scope
         * (used for tag resolving), may be NULL.
         */
        TypeSpecBuilder(CTFReaderBuilder* builder,
            CTFScope* scope, CTFType* typeConnected) : builder(builder),
            scope(scope), typeConnected(typeConnected) {}
        
        /* Main function. Return type builded. */
        const CTFType* build(const CTFASTTypeSpec& typeSpec);
        
        void visitStruct(const CTFASTStructSpec& structSpec);
        void visitInt(const CTFASTIntSpec& intSpec);
        void visitEnum(const CTFASTEnumSpec& enumSpec);
        void visitVariant(const CTFASTVariantSpec& variantSpec);
        void visitID(const CTFASTTypeIDSpec& typeIDSpec);
    protected:
        CTFReaderBuilder* builder;
        CTFScope* scope;
        CTFType* typeConnected;
        
        const CTFType* typeConstructed;
    };

    /* 
     * Get type corresponded to given specification.
     * 
     * 'scope' - current scope, 'typeConnected' - type connected to the
     * scope(may be NULL).
     */
    const CTFType* resolveTypeSpec(
        const CTFASTTypeSpec& typeSpec,
        CTFScope* scope, CTFType* typeConnected);
    /* Resolve 'struct ...' type specification */
    const CTFTypeStruct* resolveStructSpec(
        const CTFASTStructSpec& structSpec,
        CTFScope* scope);
    /* Same but for integer specification */
    const CTFTypeInt* resolveIntSpec(
        const CTFASTIntSpec& intSpec,
        CTFScope* scope);
    /* 
     * Same but for enumeration specifiaction.
     * 
     * Note: 'typeConnected' is needed for resolve base type.
     * But when this argument is not really needed, because base type
     * of enumeration cannot be variant.
     */
    const CTFTypeEnum* resolveEnumSpec(
        const CTFASTEnumSpec& enumSpec,
        CTFScope* scope, CTFType* typeConnected);
    /* Same but for variant specifiaction */
    const CTFTypeVariant* resolveVariantSpec(
        const CTFASTVariantSpec& variantSpec,
        CTFScope* scope, CTFType* typeConnected);

    /*
     * Resolve structure specification as specifiaction for new named
     * structure.
     * 
     * Used for intepret CTFASTStructDecl.
     */
    void createStruct(
        const CTFASTStructSpec& structSpec,
        CTFScope* scope);
    /* Same but for enumeration specification. */
    void createEnum(
        const CTFASTEnumSpec& enumSpec,
        CTFScope* scope, CTFType* typeConnected);
    /* Same but for variant specification. */
    void createVariant(
        const CTFASTVariantSpec& variantSpec,
        CTFScope* scope, CTFType* typeConnected);
    /* Named integer cannot be declared */
   
    
    /* 
     * Resolve tag according to root type.
     * 
     * If typeConnected is not NULL, also attempt to resolve tag
     * according to it.
     * 
     * Either return tag resolved or throw exception.
     */
    CTFTag resolveTag(const std::string& tagStr, CTFType* typeConnected);
    
    CTFReader* reader;
    
    mutable CTFTypeInt::ByteOrder boNative;
    mutable bool boNativeIsSet;
};

/************************* CTFReader constructor ********************/
CTFReader::CTFReader(std::istream& s) : uuid(NULL)
{
    typeRoot = new RootType();
    scopeRoot = new CTFScopeRoot();
    
    CTFAST ast;
    CTFReaderParser parser(s, ast);
    
    parser.parse();
    
    CTFReaderBuilder builder(*this);
    builder.build(ast);
    
    const std::string* uuidStr = findParameter("trace.uuid");
    if(uuidStr)
    {
        std::istringstream is(*uuidStr);
        uuid = new UUID();
        is >> *uuid;
        
        if(!is || (is.peek() != std::istream::traits_type::eof()))
        {
            std::cerr << "Failed to parse '" << *uuidStr << "' as UUID.\n";
            throw std::logic_error("Incorrect format of trace UUID.");
        }
        
        std::cerr << "Parsed UUID is " << *uuid << ".\n";
        
        //uuid = new unsigned char[16];
        //parseUUID(*uuidStr, uuid);
    }

    varRoot = static_cast<const RootVar*>(instantiate(typeRoot));
    
    if(uuid)
	{
		varUUID = varRoot->findVar("trace.packet.header.uuid");
		if(varUUID != NULL)
		{
			if(varUUID->getSize() != 16 * 8)
			{
				std::cerr << "Size of trace.uuid variable should be 16 byte\n";
				throw std::invalid_argument("Invalid UUID variable");
			}
		}
	}
	else varUUID = NULL;

	const CTFVar* varMagicBase = varRoot->findVar("trace.packet.header.magic");
	if(varMagicBase)
	{
		if(!varMagicBase->isInt())
		{
			std::cerr << "Type of 'trace.magic' variable should be int.\n";
			throw std::invalid_argument("Invalid magic variable type");
		}
        varMagic = static_cast<const CTFVarInt*>(varMagicBase);
        
        if(varMagic->getSize() != 4 * 8)
        {
            std::cerr << "Size of 'trace.magic' variable should be 4.\n";
            throw std::invalid_argument("Invalid magic variable size");
        }
	}
    else varMagic = NULL;
}

CTFReader::~CTFReader(void)
{
    CTFMeta::destroy();

    delete uuid;
    delete typeRoot;
    delete scopeRoot;
}

/************************* Implementation *****************************/
/* Builder itself implementation */
CTFReaderBuilder::CTFReaderBuilder(CTFReader& reader)
    : reader(&reader), boNativeIsSet(false)
{
}

CTFTypeInt::ByteOrder CTFReaderBuilder::getNativeByteOrder(void) const
{
    if(!boNativeIsSet)
    {
        const std::string* boNativeString
            = reader->findParameter("trace.byte_order");
        if(boNativeString == NULL)
            throw std::invalid_argument
                ("Native byte order is not set for the trace.");
        
        if((*boNativeString == "be") || (*boNativeString == "network"))
        {
            boNative = CTFTypeInt::be;
        }
        else if(*boNativeString == "le")
        {
            boNative =  CTFTypeInt::le;
        }
        else throw std::invalid_argument
            ("Incorrect value of parameter 'trace.byte_order' '"
                + *boNativeString + "', should be 'be', 'le' or 'network'");
    }
    return boNative;
}

void CTFReaderBuilder::build(const CTFAST& ast)
{
    RootScopeBuilder rootScopeBuilder(this);
    
    rootScopeBuilder.build(*ast.rootScope);
}


const CTFType* CTFReaderBuilder::resolveTypeSpec(
    const CTFASTTypeSpec& typeSpec,
    CTFScope* scope, CTFType* typeConnected)
{
    TypeSpecBuilder typeSpecBuilder(this, scope, typeConnected);
    return typeSpecBuilder.build(typeSpec);
}

const CTFTypeStruct* CTFReaderBuilder::resolveStructSpec(
    const CTFASTStructSpec& structSpec,
    CTFScope* scope)
{
    const CTFTypeStruct* typeStruct;
    if(structSpec.scope.get())
    {
        CTFTypeStruct* typeStructNew = reader->createTypeStruct();
        
        scope->addType(typeStructNew);
        
        CTFScope* scopeStruct = new CTFScope();
        scope->addScope(scopeStruct);
        
        TypeStructBuilder typeStructBuilder(this);
        
        typeStructBuilder.build(*structSpec.scope, typeStructNew, scopeStruct);
        
        typeStruct = typeStructNew;
        
        if(structSpec.name.get())
        {
            scope->addStructName(typeStruct, *structSpec.name);
        }
    }
    else
    {
        if(!structSpec.name.get())
            throw std::invalid_argument
                ("Structure specification without name and body");
        typeStruct = scope->findStruct(*structSpec.name);
        if(typeStruct == NULL)
            throw std::invalid_argument
                ("Unknown structure type '" + *structSpec.name + "'");
    }
    return typeStruct;
}

void CTFReaderBuilder::createStruct(
    const CTFASTStructSpec& structSpec,
    CTFScope* scope)
{
    resolveStructSpec(structSpec, scope);
    
    if(!structSpec.scope.get())
    {
        std::cerr << "Existed structure '" + *structSpec.name + "' is used, "
            + "but it is not a declaration." << std::endl;
    }
    else if(!structSpec.name.get())
    {
        std::cerr << "Unnamed structure has no effect in declaration" << std::endl;
    }
}

const CTFTypeVariant* CTFReaderBuilder::resolveVariantSpec(
    const CTFASTVariantSpec& variantSpec,
    CTFScope* scope, CTFType* typeConnected)
{
    const CTFTypeVariant* typeVariant;
    if(variantSpec.scope.get())
    {
        CTFTypeVariant* typeVariantNew = reader->createTypeVariant();
        
        scope->addType(typeVariantNew);
        
        if(variantSpec.tag.get())
        {
            CTFTag tag = resolveTag(*variantSpec.tag, typeConnected);
            typeVariantNew->setTag(tag);
        }
        
        CTFScope* scopeVariant = new CTFScope();
        scope->addScope(scopeVariant);
        
        TypeVariantBuilder typeVariantBuilder(this);
        
        typeVariantBuilder.build(*variantSpec.scope, typeVariantNew,
            scopeVariant);
        
        typeVariant = typeVariantNew;
        
        if(variantSpec.name.get())
        {
            scope->addVariantName(typeVariant, *variantSpec.name);
        }
    }
    else
    {
        if(!variantSpec.name.get())
            throw std::invalid_argument
                ("Variant specification without name and body");
        typeVariant = scope->findVariant(*variantSpec.name);
        if(!typeVariant)
            throw std::invalid_argument
                ("Unknown variant type '" + *variantSpec.name + "'");
        
        if(variantSpec.tag.get())
        {
            CTFTag tag = resolveTag(*variantSpec.tag, typeConnected);

            CTFTypeVariant* typeVariantNew = typeVariant->clone();
            scope->addType(typeVariantNew);
            typeVariantNew->setTag(tag);
            
            typeVariant = typeVariantNew;
        }
        
    }
    return typeVariant;
}

void CTFReaderBuilder::createVariant(
    const CTFASTVariantSpec& variantSpec,
    CTFScope* scope, CTFType* typeConnected)
{
    resolveVariantSpec(variantSpec, scope, typeConnected);
    
    if(!variantSpec.scope.get())
    {
        std::cerr << "Existed variant '" + *variantSpec.name + "' is used, "
            + "but it is not a declaration." << std::endl;
    }
    else if(!variantSpec.name.get())
    {
        std::cerr << "Unnamed variant has no effect in declaration" << std::endl;
    }
}


const CTFTypeEnum* CTFReaderBuilder::resolveEnumSpec(
    const CTFASTEnumSpec& enumSpec,
    CTFScope* scope, CTFType* typeConnected)
{
    const CTFTypeEnum* typeEnum;
    if(enumSpec.scope.get())
    {
        if(!enumSpec.specInt.get())
            throw std::invalid_argument("Enumeration types with body but "
                "without base integer type are not allowed.");
        
        const CTFType* typeIntBase = resolveTypeSpec(*enumSpec.specInt,
            scope, typeConnected);
        if(!typeIntBase->isInt())
            throw std::invalid_argument("Base type for enumeration is not integer.");
        
        CTFTypeEnum* typeEnumNew = reader->createTypeEnum(
            static_cast<const CTFTypeInt*>(typeIntBase));
        
        scope->addType(typeEnumNew);
        
        TypeEnumBuilder typeEnumBuilder;
        typeEnumBuilder.build(*enumSpec.scope, typeEnumNew);
        
        typeEnum = typeEnumNew;
        
        if(enumSpec.name.get())
        {
            scope->addEnumName(typeEnum, *enumSpec.name);
        }
    }
    else
    {
        if(!enumSpec.name.get())
            throw std::invalid_argument
                ("Enumeration specification without name and body.");
        typeEnum = scope->findEnum(*enumSpec.name);
        if(!typeEnum)
            throw std::invalid_argument("Unknown enumeration type '"
                + *enumSpec.name + "'");
        if(enumSpec.specInt.get())
            throw std::invalid_argument("Redefinition base integer type "
                "for enumeration is not allowed.");
    }
    return typeEnum;
}

void CTFReaderBuilder::createEnum(
    const CTFASTEnumSpec& enumSpec,
    CTFScope* scope, CTFType* typeConnected)
{
    resolveEnumSpec(enumSpec, scope, typeConnected);
    
    if(!enumSpec.scope.get())
    {
        std::cerr << "Existed enumeration '" + *enumSpec.name + "' is used, "
            + "but it is not a declaration." << std::endl;
    }
    else if(!enumSpec.name.get())
    {
        std::cerr << "Unnamed enumeration has no effect in declaration" << std::endl;
    }
}

const CTFTypeInt* CTFReaderBuilder::resolveIntSpec(
    const CTFASTIntSpec& intSpec,
    CTFScope* scope)
{
    if(!intSpec.scope.get())
        throw std::invalid_argument("Integer specification without scope.");
    
    CTFTypeInt* typeInt = reader->createTypeInt();
    scope->addType(typeInt);
    
    CTFReaderBuilder::TypeIntBuilder typeIntBuilder(this);
    
    typeIntBuilder.build(*intSpec.scope, typeInt);
    
    return typeInt;
}
        
CTFTag CTFReaderBuilder::resolveTag(const std::string& tagStr,
    CTFType* typeConnected)
{
    if(typeConnected)
    {
        CTFTag tagRelative = typeConnected->resolveTag(tagStr);
        if(tagRelative.isConnected()) return tagRelative;
    }
    CTFTag tagAbsolute = reader->typeRoot->resolveTag(tagStr);
    if(!tagAbsolute.isConnected())
        throw std::invalid_argument
            ("Failed to resolve tag '" + tagStr + "'");
    return tagAbsolute;
}
/* Builder for integer type */
void CTFReaderBuilder::TypeIntBuilder::build
    (const CTFASTScopeInt& scopeInt, CTFTypeInt* typeInt)
{
    this->typeInt = typeInt;
    byteOrderIsSet = false;
    
    CTFASTScope::iterator iter;
    for(iter = scopeInt.begin(); iter != scopeInt.end(); iter++)
    {
        visit(**iter);
    }
    
    if(!byteOrderIsSet)
    {
        typeInt->setByteOrder(builder->getNativeByteOrder());
        byteOrderIsSet = true;
    }
    
    typeInt->fixParams();
}

void CTFReaderBuilder::TypeIntBuilder::visitParameterDef
    (const CTFASTParameterDef& parameterDef)
{
    const std::string& name = *parameterDef.paramName;
    const std::string& value = *parameterDef.paramValue;

#define param_is(str) (name == str)
#define value_is(str) (value == str)

    if(param_is("byte_order"))
    {
        CTFTypeInt::ByteOrder bo;
        if(value_is("be") || value_is("network"))
            bo = CTFTypeInt::be;
        else if(value_is("le"))
            bo = CTFTypeInt::le;
        else if(value_is("native"))
            bo = builder->getNativeByteOrder();
        else
            throw std::invalid_argument("Unknown value of 'byte_order' integer parameter '"
                + value + "', should be 'le', 'be', 'network' or 'native'");
        
        typeInt->setByteOrder(bo);
        byteOrderIsSet = true;
    }
    else if(param_is("signed"))
    {
        int is_signed;

        if(value_is("true"))
            is_signed = 1;
        else if(value_is("false"))
            is_signed = 0;
        else
            throw std::invalid_argument("Unknown value of 'signed' integer parameter '"
                + value + "', should be 'true' or 'false'");

        typeInt->setSigned(is_signed);
    }
    else if(param_is("size"))
    {
        std::istringstream ss(value);
        
        int size;
        ss >> size;
        if(!ss.eof())
            throw std::invalid_argument
                ("Exceeded characters in value of 'size' integer parameter");
        typeInt->setSize(size);
    }
    else if(param_is("align"))
    {
        std::istringstream ss(value);
        
        int align;
        ss >> align;
        if(!ss.eof())
            throw std::invalid_argument
                ("Exceeded characters in value of 'align' integer parameter");
        typeInt->setAlignment(align);
    }
    else if(param_is("base"))
    {
        /* Currently ignored */
    }
    else if(param_is("encoding"))
    {
        if(!value_is("none"))
        {
            throw std::invalid_argument
                ("Encodings other than 'none' currently not supported");
        }
    }
    else
    {
        throw std::invalid_argument("Unknown integer parameter '"
            + name + "'");
    }
#undef param_is
#undef value_is
}

/* Builder for enumeration type */
void CTFReaderBuilder::TypeEnumBuilder::build(
    const CTFASTScopeEnum& scopeEnum, CTFTypeEnum* typeEnum)
{
    this->typeEnum = typeEnum;
    nextValue = 0;
    
    CTFASTScopeEnum::iterator iter;
    for(iter = scopeEnum.begin(); iter != scopeEnum.end(); ++iter)
    {
        visit(**iter);
    }
}

void CTFReaderBuilder::TypeEnumBuilder::visitSimple(
    const CTFASTEnumValueDeclSimple& enumValueDeclSimple)
{
    typeEnum->addValue64(enumValueDeclSimple.name->c_str(),
        nextValue, nextValue);

    ++nextValue;
}

void CTFReaderBuilder::TypeEnumBuilder::visitPresize(
    const CTFASTEnumValueDeclPresize& enumValueDeclPresize)
{
    std::istringstream ss(*enumValueDeclPresize.value);
    
    int64_t v;
    ss >> v;
    if(!ss.eof())
        throw std::invalid_argument
            ("Exceeded characters in enumeration value.");
        
    typeEnum->addValue64(enumValueDeclPresize.name->c_str(),
        v, v);

    nextValue = v + 1;
}

void CTFReaderBuilder::TypeEnumBuilder::visitRange(
    const CTFASTEnumValueDeclRange& enumValueDeclRange)
{
    std::istringstream ssStart(*enumValueDeclRange.valueStart);
    
    int64_t vStart;
    ssStart >> vStart;
    if(!ssStart.eof())
        throw std::invalid_argument
            ("Exceeded characters in enumeration starting value.");
    
    std::istringstream ssEnd(*enumValueDeclRange.valueEnd);
    
    int64_t vEnd;
    ssEnd >> vEnd;
    if(!ssEnd.eof())
        throw std::invalid_argument
            ("Exceeded characters in enumeration ending value.");
        
    typeEnum->addValue64(enumValueDeclRange.name->c_str(),
        vStart, vEnd);

    nextValue = vEnd + 1;
}

/* Type modificators builder */
const CTFType* CTFReaderBuilder::TypePostModsBuilder::build(
    const CTFASTTypePostMods& typeInst, const CTFType* typeInitial)
{
    typeCurrent = typeInitial;
    
    CTFASTTypePostMods::iterator iter;
    for(iter = typeInst.begin(); iter != typeInst.end(); ++iter)
    {
        visit(**iter);
    }
    
    return typeCurrent;
}

void CTFReaderBuilder::TypePostModsBuilder::visitArray(
    const CTFASTArrayMod& arrayMod)
{
    std::istringstream ss(*arrayMod.sizeStr);
    
    int size;
    ss >> size;
    if(!ss.eof())
        throw std::invalid_argument("Exceeded characters in array size.");
    
    CTFTypeArray* typeArray
        = builder->reader->createTypeArray(size, typeCurrent);
    scope->addType(typeArray);
    
    typeCurrent = typeArray;
}

void CTFReaderBuilder::TypePostModsBuilder::visitSequence(
    const CTFASTSequenceMod& sequenceMod)
{
    CTFTag sizeTag = builder->resolveTag(*sequenceMod.sizeTagStr,
        typeConnected);
    
    CTFTypeSequence* typeSequence
        = builder->reader->createTypeSequence(sizeTag, typeCurrent);
    
    scope->addType(typeSequence);
    
    typeCurrent = typeSequence;
}


/* Scope builder */
void CTFReaderBuilder::ScopeBuilder::build(const CTFASTScope& scope,
    CTFScope* scopeBuilded, CTFType* typeConnected)
{
    this->scope = scopeBuilded;
    this->typeConnected = typeConnected;
    
    CTFASTScope::iterator iter;
    for(iter = scope.begin(); iter != scope.end(); ++iter)
    {
        visit(**iter);
    }
}

void CTFReaderBuilder::ScopeBuilder::visitStructDecl(
    const CTFASTStructDecl& structDecl)
{
    builder->createStruct(*structDecl.structSpec, scope);
}

void CTFReaderBuilder::ScopeBuilder::visitVariantDecl(
    const CTFASTVariantDecl& variantDecl)
{
    builder->createVariant(*variantDecl.variantSpec, scope, typeConnected);
}

void CTFReaderBuilder::ScopeBuilder::visitEnumDecl(
    const CTFASTEnumDecl& enumDecl)
{
    builder->createEnum(*enumDecl.enumSpec, scope, typeConnected);
}

void CTFReaderBuilder::ScopeBuilder::visitTypedefDecl(
    const CTFASTTypedefDecl& typedefDecl)
{
    const CTFType* baseType = builder->resolveTypeSpec(
        *typedefDecl.typeSpec, scope, typeConnected);
    /* One builder for all type instantiations */
    CTFReaderBuilder::TypePostModsBuilder typePostModsBuilder(builder,
        scope, typeConnected);
    
    CTFASTTypedefDecl::iterator iter;
    for(iter = typedefDecl.begin(); iter != typedefDecl.end(); ++iter)
    {
        const CTFASTTypedefDecl::TypeInst& typeInst = **iter;
        const CTFType* type = typePostModsBuilder.build(
            *typeInst.postMods, baseType);
        scope->addTypeName(type, *typeInst.name);
    }
}

/* Structure type builder */
void CTFReaderBuilder::TypeStructBuilder::build(
    const CTFASTScopeStruct& scopeStruct,
    CTFTypeStruct* typeStruct, CTFScope* scope)
{
    this->typeStruct = typeStruct;
    ScopeBuilder::build(scopeStruct, scope, typeStruct);
}

void CTFReaderBuilder::TypeStructBuilder::visitFieldDecl(
    const CTFASTFieldDecl& fieldDecl)
{
    const CTFType* baseType = builder->resolveTypeSpec(
        *fieldDecl.typeSpec, scope, typeConnected);
    
    /* One builder for all type instantiations */
    CTFReaderBuilder::TypePostModsBuilder typePostModsBuilder(builder,
        scope, typeConnected);

    CTFASTFieldDecl::iterator iter;
    for(iter = fieldDecl.begin(); iter != fieldDecl.end(); ++iter)
    {
        const CTFASTFieldDecl::TypeInst& typeInst = **iter;
        const CTFType* type = typePostModsBuilder.build(
            *typeInst.postMods, baseType);
        typeStruct->addField(*typeInst.name, type);
    }
}

/* Variant type builder */
void CTFReaderBuilder::TypeVariantBuilder::build(
    const CTFASTScopeVariant& scopeVariant,
    CTFTypeVariant* typeVariant, CTFScope* scope)
{
    this->typeVariant = typeVariant;
    ScopeBuilder::build(scopeVariant, scope, typeVariant);
}

void CTFReaderBuilder::TypeVariantBuilder::visitFieldDecl(
    const CTFASTFieldDecl& fieldDecl)
{
    const CTFType* baseType = builder->resolveTypeSpec(
        *fieldDecl.typeSpec, scope, typeConnected);
    
    /* One builder for all type instantiations */
    CTFReaderBuilder::TypePostModsBuilder typePostModsBuilder(builder,
        scope, typeConnected);

    CTFASTFieldDecl::iterator iter;
    for(iter = fieldDecl.begin(); iter != fieldDecl.end(); ++iter)
    {
        const CTFASTFieldDecl::TypeInst& typeInst = **iter;
        const CTFType* type = typePostModsBuilder.build(
            *typeInst.postMods, baseType);
        typeVariant->addField(*typeInst.name, type);
    }
}


/* Top scope builder */
void CTFReaderBuilder::TopScopeBuilder::build(
    const CTFASTScopeTop& scopeTop,
    CTFScopeTop* scope, const std::string& name)
{
    this->scope = scope;
    this->name = &name;
    
    ScopeBuilder::build(scopeTop, scope, NULL);
}

void CTFReaderBuilder::TopScopeBuilder::visitParameterDef(
    const CTFASTParameterDef& parameterDef)
{
    scope->addParameter(*parameterDef.paramName,
        *parameterDef.paramValue);
}

void CTFReaderBuilder::TopScopeBuilder::visitTypeAssignment(
    const CTFASTTypeAssignment& typeAssignment)
{
    const CTFType* baseType = builder->resolveTypeSpec
        (*typeAssignment.typeSpec, scope, typeConnected);
    
    TypePostModsBuilder typePostModsBuilder(builder, scope, typeConnected);
    const CTFType* type = typePostModsBuilder.build(
        *typeAssignment.postMods, baseType);
    
    builder->reader->typeRoot->assignType
        (*name + "." + *typeAssignment.position, type);
}

/* Root scope builder */
void CTFReaderBuilder::RootScopeBuilder::build(
    const CTFASTScopeRoot& scopeRoot)
{
    ScopeBuilder::build(scopeRoot, builder->reader->scopeRoot,
        builder->reader->typeRoot);
}

void CTFReaderBuilder::RootScopeBuilder::visitTopScopeDecl(
    const CTFASTTopScopeDecl& topScopeDecl)
{
    CTFScopeRoot* scopeRoot = builder->reader->scopeRoot;
    
    CTFScopeTop* scopeTop = new CTFScopeTop();
    scopeRoot->addScope(scopeTop);
    scopeRoot->addTopScopeName(scopeTop, *topScopeDecl.name);
    
    TopScopeBuilder topScopeBuilder(builder);
    
    topScopeBuilder.build(*topScopeDecl.scope, scopeTop, *topScopeDecl.name);
}

/* Type spec builder */
const CTFType* CTFReaderBuilder::TypeSpecBuilder::build(
    const CTFASTTypeSpec& typeSpec)
{
    visit(typeSpec);
    return typeConstructed;
}

void CTFReaderBuilder::TypeSpecBuilder::visitStruct(
    const CTFASTStructSpec& structSpec)
{
    typeConstructed = builder->resolveStructSpec(structSpec, scope);
}

void CTFReaderBuilder::TypeSpecBuilder::visitVariant(
    const CTFASTVariantSpec& variantSpec)
{
    typeConstructed = builder->resolveVariantSpec(variantSpec, scope,
        typeConnected);
}

void CTFReaderBuilder::TypeSpecBuilder::visitEnum(
    const CTFASTEnumSpec& enumSpec)
{
    typeConstructed = builder->resolveEnumSpec(enumSpec, scope,
        typeConnected);
}

void CTFReaderBuilder::TypeSpecBuilder::visitInt(
    const CTFASTIntSpec& intSpec)
{
    typeConstructed = builder->resolveIntSpec(intSpec, scope);
}

void CTFReaderBuilder::TypeSpecBuilder::visitID(
    const CTFASTTypeIDSpec& typeIDSpec)
{
    typeConstructed = scope->findType(*typeIDSpec.id);
    if(!typeConstructed)
        throw std::invalid_argument
            ("Unknown type '" + *typeIDSpec.id + "'.");
}

/**********************************************************************/
/*void parseUUID(const std::string& uuidStr, unsigned char* uuid)
{
    static unsigned char hexValues[256];
    static bool hexValuesInitialized = false;
    if(!hexValuesInitialized)
    {
        for(int i = 0; i < 256; i++)
        {
            if((i >= '0') && (i <= '9'))
                hexValues[i] = (unsigned char)(i - '0');
            else if((i >= 'a') && (i <= 'f'))
                hexValues[i] = (unsigned char)(i - 'a' + 10);
            else if((i >= 'A') && (i <= 'F'))
                hexValues[i] = (unsigned char)(i - 'A' + 10);
            else
                hexValues[i] = 0xffu;
        }
        hexValuesInitialized = true;
    }
    
    int uuidCurrentByte = 0;
    for(const char* uuidCurrentChar = uuidStr.c_str();
        *uuidCurrentChar != '\0';
        uuidCurrentChar++)
    {
        if(isxdigit(uuidCurrentChar[0]))
        {
            if(!isxdigit(uuidCurrentChar[1]))
            {
                std::cerr << "Cannot interpret string '" << uuidStr
                    << " ' as UUID.\n";
                throw std::invalid_argument("Invalid UUID string.");
            }
            if(uuidCurrentByte >= 16)
            {
                std::cerr << "Cannot interpret string '" << uuidStr
                    << " ' as UUID.\n";
                throw std::invalid_argument("Invalid UUID string.");
            }
            uuid[++uuidCurrentByte]
                = hexValues[(int)uuidCurrentChar[0]] << 4
                + hexValues[(int)uuidCurrentChar[1]];
            uuidCurrentChar++;
        }
        else if(uuidCurrentChar[0] != '-')
        {
            std::cerr << "Cannot interpret string '" << uuidStr
                << " ' as UUID.\n";
            throw std::invalid_argument("Invalid UUID string.");
        }
    }
    
    if(uuidCurrentByte != 16)
    {
        std::cerr << "Cannot interpret string '" << uuidStr
            << " ' as UUID.\n";
        throw std::invalid_argument("Invalid UUID string.");
    }
}*/
