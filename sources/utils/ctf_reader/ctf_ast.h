/*
 * Abstract syntax tree when parsing CTF metadata.
 */
#ifndef CTF_AST_H_INCLUDED
#define CTF_AST_H_INCLUDED

#include <vector>
#include <string>
#include <cstdlib> /* malloc/free */
#include <cassert> /* assert() */
#include <memory> /* auto_ptr */

/* Vector of pointers to objects, which should be deleted when no needed.*/
template<class T>
class CTFAutoVector
{
public:
    CTFAutoVector(void) {};
    ~CTFAutoVector(void)
    {
        for(typename std::vector<T*>::iterator iter = v.begin();
            iter != v.end(); ++iter)
            delete *iter;
    }
    void push_back(std::auto_ptr<T> t)
    {
        v.push_back(t.get());
        t.release();
    }
    
    typedef typename std::vector<T*>::const_iterator iterator;
    
    iterator begin(void) const {return v.begin();};
    iterator end(void) const {return v.end();};
private:
    CTFAutoVector(const CTFAutoVector&);
    CTFAutoVector& operator=(const CTFAutoVector&);
    
    std::vector<T*> v;
};


class CTFASTStatement;

/* One parsing scope, which contains statements*/
class CTFASTScopeRoot;
class CTFASTScopeTop;
class CTFASTScopeStruct;
class CTFASTScopeVariant;
class CTFASTScopeInt;
class CTFASTScopeEnum;

class CTFASTScope
{
public:
    ~CTFASTScope(void) {}

    /* Iterator throw statements */
    typedef CTFAutoVector<CTFASTStatement>::iterator iterator;
    /* Add statement to the back of the scope. */
    void addStatement(std::auto_ptr<CTFASTStatement> statement)
        {statements.push_back(statement);}
    
    iterator begin(void) const {return statements.begin();}
    iterator end(void) const {return statements.end();}

private:
    CTFAutoVector<CTFASTStatement> statements;
};

/* One statement in the scope */
class CTFASTStructDecl;
class CTFASTVariantDecl;
class CTFASTEnumDecl;
class CTFASTTypedefDecl;
class CTFASTFieldDecl;
class CTFASTParameterDef;
class CTFASTTopScopeDecl;
class CTFASTTypeAssignment;

class CTFASTStatement
{
public:
    virtual ~CTFASTStatement(void) {};
    
    class Visitor
    {
    public:
        void visit(const CTFASTStatement& statement)
            {statement.visit(*this);};
        
        virtual void visitStructDecl
            (const CTFASTStructDecl& structDecl);
        virtual void visitVariantDecl
            (const CTFASTVariantDecl& variantDecl);
        virtual void visitEnumDecl
            (const CTFASTEnumDecl& enumDecl);
        virtual void visitTypedefDecl
            (const CTFASTTypedefDecl& typedefDecl);
        virtual void visitFieldDecl
            (const CTFASTFieldDecl& fieldDecl);
        virtual void visitParameterDef
            (const CTFASTParameterDef& parameterDef);
        virtual void visitTopScopeDecl
            (const CTFASTTopScopeDecl& topScopeDecl);
        virtual void visitTypeAssignment
            (const CTFASTTypeAssignment& typeAssignment);
    };
protected:
    virtual void visit(Visitor& visitor) const = 0;
};

/* Type specificator */
class CTFASTTypeIDSpec;
class CTFASTStructSpec;
class CTFASTIntSpec;
class CTFASTVariantSpec;
class CTFASTEnumSpec;

class CTFASTTypeSpec
{
public:
    virtual ~CTFASTTypeSpec(void) {};
    
    class Visitor
    {
    public:
        void visit(const CTFASTTypeSpec& typeSpec)
            {return typeSpec.visit(*this); }
        
        virtual void visitID(const CTFASTTypeIDSpec& typeIDSpec);
        virtual void visitStruct(const CTFASTStructSpec& structSpec);
        virtual void visitInt(const CTFASTIntSpec& intSpec);
        virtual void visitVariant
            (const CTFASTVariantSpec& variantSpec);
        virtual void visitEnum(const CTFASTEnumSpec& enumSpec);
    };
protected:
    virtual void visit(Visitor& visitor) const = 0;
};

/* 
 * Post modificator for type.
 * 
 * This modificators follows field or typedef identificator and
 * means array or sequence ("[...]").
 */
class CTFASTArrayMod;
class CTFASTSequenceMod;

class CTFASTTypePostMod
{
public:
    virtual ~CTFASTTypePostMod(void) {}
    
    class Visitor
    {
    public:
        void visit(const CTFASTTypePostMod& typePostMod)
            {return typePostMod.visit(*this);}
        
        virtual void visitArray(const CTFASTArrayMod& arrayMod) = 0;
        virtual void visitSequence(const CTFASTSequenceMod& sequenceMod) = 0;
    };
protected:
    virtual void visit(Visitor& visitor) const = 0;
};

/*
 * Zero or more post parameters of type.
 * 
 * Used in field declaration, typedefs and type assignment.
 */
class CTFASTTypePostMods
{
public:
    void addTypePostMod(std::auto_ptr<CTFASTTypePostMod> typePostMod)
        {mods.push_back(typePostMod);}

    typedef CTFAutoVector<CTFASTTypePostMod>::iterator iterator;
    iterator begin(void) const {return mods.begin();}
    iterator end(void) const {return mods.end();}
private:
    CTFAutoVector<CTFASTTypePostMod> mods;
};


/*
 * Declaration of enumeration values.
 */
class CTFASTEnumValueDeclSimple;
class CTFASTEnumValueDeclPresize;
class CTFASTEnumValueDeclRange;

class CTFASTEnumValueDecl
{
public:
    std::auto_ptr<std::string> name;

    CTFASTEnumValueDecl(std::auto_ptr<std::string> name)
        : name(name) {}
    virtual ~CTFASTEnumValueDecl(void) {}
    
    class Visitor
    {
    public:
        void visit(const CTFASTEnumValueDecl& enumValueDecl)
            {return enumValueDecl.visit(*this);}
        
        virtual void visitSimple
            (const CTFASTEnumValueDeclSimple& enumValueDeclSimple) = 0;
        virtual void visitPresize
            (const CTFASTEnumValueDeclPresize& enumValueDeclPresize) = 0;
        virtual void visitRange
            (const CTFASTEnumValueDeclRange& enumValueDeclRange) = 0;
    };
protected:
    virtual void visit(Visitor& visitor) const = 0;
};

/******************** Concrete specializations ************************/

/* Root scope */
class CTFASTScopeRoot : public CTFASTScope
{
};

/* Top scope and its declaration */
class CTFASTScopeTop : public CTFASTScope
{
public:
    CTFASTTopScopeDecl* decl;

    CTFASTScopeTop(void) : decl(NULL) {}
};

class CTFASTTopScopeDecl: public CTFASTStatement
{
public:
    std::auto_ptr<std::string> name;
    std::auto_ptr<CTFASTScopeTop> scope;

    CTFASTTopScopeDecl(std::auto_ptr<std::string> name,
        std::auto_ptr<CTFASTScopeTop> _scope)
        : name(name), scope(_scope) {scope->decl = this;}
protected:
    void visit(Visitor& visitor) const
        {visitor.visitTopScopeDecl(*this);}
};

/* Struct scope and its specification*/
class CTFASTScopeStruct: public CTFASTScope
{
public:
    CTFASTStructSpec* spec;

    CTFASTScopeStruct(void) : spec(NULL) {}
};

class CTFASTStructSpec: public CTFASTTypeSpec
{
public:
    std::auto_ptr<std::string> name; /* NULL if unnamed */
    std::auto_ptr<CTFASTScopeStruct> scope;/* NULL if not exist */

    CTFASTStructSpec(
        std::auto_ptr<std::string> name,
        std::auto_ptr<CTFASTScopeStruct> _scope)
        : name(name), scope(_scope)
        {scope->spec = this;}
    CTFASTStructSpec(std::auto_ptr<std::string> name)
        : name(name), scope(NULL) {}
    CTFASTStructSpec(std::auto_ptr<CTFASTScopeStruct> _scope)
        : name(NULL), scope(_scope) {scope->spec = this;}
    /* Call CTFASTStructSpec(NULL) cause unresolved collision */
    
protected:
    void visit(Visitor& visitor) const {visitor.visitStruct(*this);}
};

/* Variant scope and its specification*/
class CTFASTScopeVariant: public CTFASTScope
{
public:
    CTFASTVariantSpec* spec;

    CTFASTScopeVariant(void) : spec(NULL) {}
};

class CTFASTVariantSpec: public CTFASTTypeSpec
{
public:
    std::auto_ptr<std::string> name; /* NULL if unnamed */
    std::auto_ptr<std::string> tag; /* NULL if untagged */
    std::auto_ptr<CTFASTScopeVariant> scope;/* NULL if not exist */

    /* 
     * Note: some parameters combinations may be incorrect.
     */
    CTFASTVariantSpec(
        std::auto_ptr<std::string> name,
        std::auto_ptr<std::string> tag,
        std::auto_ptr<CTFASTScopeVariant> _scope)
        : name(name), tag(tag), scope(_scope)
        {if(scope.get()) scope->spec = this;}
        
protected:
    void visit(Visitor& visitor) const {visitor.visitVariant(*this);}
};


/* Integer scope and its specification*/
class CTFASTScopeInt: public CTFASTScope
{
public:
    CTFASTIntSpec* spec;

    CTFASTScopeInt(void) : spec(NULL) {}
};

class CTFASTIntSpec: public CTFASTTypeSpec
{
public:
    std::auto_ptr<CTFASTScopeInt> scope;

    CTFASTIntSpec(std::auto_ptr<CTFASTScopeInt> _scope)
        : scope(_scope) {scope->spec = this;}
protected:
    void visit(Visitor& visitor) const {visitor.visitInt(*this);}
};

/* Enumeration scope and its specification*/
class CTFASTScopeEnum: public CTFASTScope
{
public:
    CTFASTEnumSpec* spec;

    CTFASTScopeEnum(void) : spec(NULL) {}
    
    void addValueDecl(std::auto_ptr<CTFASTEnumValueDecl> enumValueDecl)
        {valueDecls.push_back(enumValueDecl);}

    typedef CTFAutoVector<CTFASTEnumValueDecl>::iterator iterator;
    iterator begin(void) const {return valueDecls.begin();}
    iterator end(void) const {return valueDecls.end();}
private:
    CTFAutoVector<CTFASTEnumValueDecl> valueDecls;
};

class CTFASTEnumSpec: public CTFASTTypeSpec
{
public:
    std::auto_ptr<std::string> name; /* NULL if unnamed */
    std::auto_ptr<CTFASTScopeEnum> scope; /* NULL if not exist */
    std::auto_ptr<CTFASTTypeSpec> specInt; /* NULL if not exist */

    /* 
     * Note: some parameters combinations may be incorrect.
     */
    CTFASTEnumSpec(
        std::auto_ptr<std::string> name,
        std::auto_ptr<CTFASTScopeEnum> _scope,
        std::auto_ptr<CTFASTTypeSpec> specInt)
            : name(name), scope(_scope), specInt(specInt)
        { if(scope.get()) scope->spec = this;}
    
protected:
    void visit(Visitor& visitor) const {visitor.visitEnum(*this);}
};

/*
 * Specification of type using only type identificator.
 */
class CTFASTTypeIDSpec: public CTFASTTypeSpec
{
public:
    std::auto_ptr<std::string> id;

    CTFASTTypeIDSpec(std::auto_ptr<std::string> id): id(id) {}
protected:
    void visit(Visitor& visitor) const {visitor.visitID(*this);}
};


/* Structure declaration */
class CTFASTStructDecl: public CTFASTStatement
{
public:
    std::auto_ptr<CTFASTStructSpec> structSpec;
    
    CTFASTStructDecl(std::auto_ptr<CTFASTStructSpec> structSpec)
        : structSpec(structSpec) {}
protected:
    void visit(Visitor& visitor) const {visitor.visitStructDecl(*this);}
};

/* Variant declaration */
class CTFASTVariantDecl: public CTFASTStatement
{
public:
    std::auto_ptr<CTFASTVariantSpec> variantSpec;
    
    CTFASTVariantDecl(std::auto_ptr<CTFASTVariantSpec> variantSpec)
        : variantSpec(variantSpec) {}
protected:
    void visit(Visitor& visitor) const {visitor.visitVariantDecl(*this);}
};

/* Enumeration declaration */
class CTFASTEnumDecl: public CTFASTStatement
{
public:
    std::auto_ptr<CTFASTEnumSpec> enumSpec;

    CTFASTEnumDecl(std::auto_ptr<CTFASTEnumSpec> enumSpec)
        : enumSpec(enumSpec) {}
protected:
    void visit(Visitor& visitor) const {visitor.visitEnumDecl(*this);}
};

/* Typedef declaration */
class CTFASTTypedefDecl: public CTFASTStatement
{
public:
    std::auto_ptr<CTFASTTypeSpec> typeSpec;
    /* One type instantiation as typedef */
    class TypeInst
    {
    public:
        std::auto_ptr<std::string> name;
        std::auto_ptr<CTFASTTypePostMods> postMods;

        TypeInst(std::auto_ptr<std::string> name,
                std::auto_ptr<CTFASTTypePostMods> postMods)
            : name(name), postMods(postMods) {}
    };

    CTFASTTypedefDecl(
        std::auto_ptr<CTFASTTypeSpec> typeSpec,
        std::auto_ptr<TypeInst> typeInst1)
        : typeSpec(typeSpec) {addTypeInst(typeInst1);}
    
    /* Add type instantiation directive - name and optional type modifiers*/
    void addTypeInst(std::auto_ptr<TypeInst> typeInst)
        {insts.push_back(typeInst);}

    typedef CTFAutoVector<TypeInst>::iterator iterator;
    iterator begin(void) const {return insts.begin(); }
    iterator end(void) const {return insts.end(); }
protected:
    void visit(Visitor& visitor) const {visitor.visitTypedefDecl(*this);}
private:
    CTFAutoVector<TypeInst> insts;
};


/* Declaration of structure or variant field(s) */
class CTFASTFieldDecl: public CTFASTStatement
{
public:
    std::auto_ptr<CTFASTTypeSpec> typeSpec;
    /* One type instantiation as field. */
    class TypeInst
    {
    public:
        std::auto_ptr<std::string> name;
        std::auto_ptr<CTFASTTypePostMods> postMods;
        
        TypeInst(std::auto_ptr<std::string> name,
            std::auto_ptr<CTFASTTypePostMods> postMods)
            : name(name), postMods(postMods) {}
        TypeInst(std::auto_ptr<std::string> name)
            : name(name), postMods(NULL) {}
    };

    CTFASTFieldDecl(
        std::auto_ptr<CTFASTTypeSpec> typeSpec,
        std::auto_ptr<TypeInst> typeInst1)
        : typeSpec(typeSpec) {addTypeInst(typeInst1);}
    
    /* Add type instantiation directive - name and optional type modifiers*/
    void addTypeInst(std::auto_ptr<TypeInst> typeInst)
        {insts.push_back(typeInst);}

    typedef CTFAutoVector<TypeInst>::iterator iterator;
    iterator begin(void) const {return insts.begin(); }
    iterator end(void) const {return insts.end(); }
protected:
    void visit(Visitor& visitor) const {visitor.visitFieldDecl(*this);}
private:
    CTFAutoVector<TypeInst> insts;
};

/* Definition of parameter */
class CTFASTParameterDef: public CTFASTStatement
{
public:
    std::auto_ptr<std::string> paramName;
    std::auto_ptr<std::string> paramValue;

    CTFASTParameterDef(
        std::auto_ptr<std::string> paramName,
        std::auto_ptr<std::string> paramValue)
        : paramName(paramName), paramValue(paramValue) {}
protected:
    void visit(Visitor& visitor) const
        {visitor.visitParameterDef(*this);}
};

/* Type assignment statement */
class CTFASTTypeAssignment: public CTFASTStatement
{
public:
    std::auto_ptr<std::string> position;

    std::auto_ptr<CTFASTTypeSpec> typeSpec;
    std::auto_ptr<CTFASTTypePostMods> postMods;

    CTFASTTypeAssignment(
        std::auto_ptr<std::string> position,
        std::auto_ptr<CTFASTTypeSpec> typeSpec,
        std::auto_ptr<CTFASTTypePostMods> postMods)
        : position(position), typeSpec(typeSpec), postMods(postMods) {}
protected:
    void visit(Visitor& visitor) const
        {visitor.visitTypeAssignment(*this);}
};

/* Type post modificator for arrays */
class CTFASTArrayMod: public CTFASTTypePostMod
{
public:
    std::auto_ptr<std::string> sizeStr;

    CTFASTArrayMod(std::auto_ptr<std::string> sizeStr)
        : sizeStr(sizeStr) {}
protected:
    void visit(Visitor& visitor) const {visitor.visitArray(*this);}
};

/* Type post modificator for sequences */
class CTFASTSequenceMod: public CTFASTTypePostMod
{
public:
    std::auto_ptr<std::string> sizeTagStr;

    CTFASTSequenceMod(std::auto_ptr<std::string> sizeTagStr)
        : sizeTagStr(sizeTagStr) {}
protected:
    void visit(Visitor& visitor) const {visitor.visitSequence(*this);}
};

/* Declaration of enumeration value in simple form. */
class CTFASTEnumValueDeclSimple: public CTFASTEnumValueDecl
{
public:
    CTFASTEnumValueDeclSimple(std::auto_ptr<std::string> name)
        : CTFASTEnumValueDecl(name) {}
    
protected:
    void visit(Visitor& visitor) const {visitor.visitSimple(*this);}
};

/* Declaration of enumeration value in presize form(as in C). */
class CTFASTEnumValueDeclPresize: public CTFASTEnumValueDecl
{
public:
    std::auto_ptr<std::string> value;

    CTFASTEnumValueDeclPresize(
        std::auto_ptr<std::string> name,
        std::auto_ptr<std::string> value)
        : CTFASTEnumValueDecl(name), value(value) {}
protected:
    void visit(Visitor& visitor) const {visitor.visitPresize(*this);}
};

/* Declaration of enumeration value in range form. */
class CTFASTEnumValueDeclRange: public CTFASTEnumValueDecl
{
public:
    std::auto_ptr<std::string> valueStart;
    std::auto_ptr<std::string> valueEnd;

    CTFASTEnumValueDeclRange(
        std::auto_ptr<std::string> name,
        std::auto_ptr<std::string> valueStart,
        std::auto_ptr<std::string> valueEnd)
        : CTFASTEnumValueDecl(name),
        valueStart(valueStart), valueEnd(valueEnd) {}
protected:
    void visit(Visitor& visitor) const {visitor.visitRange(*this);}
};


/************************** AST itself ********************************/
class CTFAST
{
public:
    CTFAST(void) : rootScope(new CTFASTScopeRoot()) {}
    ~CTFAST(void) {delete rootScope;}

    CTFASTScopeRoot* rootScope;
};

#endif /* CTF_AST_H_INCLUDED */