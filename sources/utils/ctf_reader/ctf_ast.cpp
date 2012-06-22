#include "ctf_ast.h"

#include <stdexcept>
#include <string>

static void err_st(const std::string& statement)
{
	throw std::logic_error("Statement '" + statement
		+ "' cannot be defined here.");
}

void CTFASTStatement::Visitor::visitStructDecl
	(const CTFASTStructDecl& structDecl)
{
	(void)structDecl;
	err_st("structure declaration");
}
void CTFASTStatement::Visitor::visitVariantDecl
	(const CTFASTVariantDecl& variantDecl)
{
	(void)variantDecl;
	err_st("variant declaration");
}
void CTFASTStatement::Visitor::visitEnumDecl
	(const CTFASTEnumDecl& enumDecl)
{
	(void)enumDecl;
	err_st("enumeration declaration");
}
void CTFASTStatement::Visitor::visitTypedefDecl
	(const CTFASTTypedefDecl& typedefDecl)
{
	(void)typedefDecl;
	err_st("type definition");
}

void CTFASTStatement::Visitor::visitFieldDecl
	(const CTFASTFieldDecl& fieldDecl)
{
	(void)fieldDecl;
	err_st("field declaration");
}

void CTFASTStatement::Visitor::visitParameterDef
	(const CTFASTParameterDef& parameterDef)
{
	(void)parameterDef;
	err_st("parameter definition");
}

void CTFASTStatement::Visitor::visitTopScopeDecl
	(const CTFASTTopScopeDecl& topScopeDecl)
{
	(void)topScopeDecl;
	err_st("top scope declaration");
}

void CTFASTStatement::Visitor::visitTypeAssignment
	(const CTFASTTypeAssignment& typeAssignment)
{
	(void)typeAssignment;
	err_st("type assignment");
}


static void err_ts(const std::string& typeSpecName)
{
	throw std::logic_error(typeSpecName + " cannot be defined here");
}

void CTFASTTypeSpec::Visitor::visitID(const CTFASTTypeIDSpec& typeIDSpec)
{
	(void)typeIDSpec;
	err_ts("Type specification using type-id");
}
void CTFASTTypeSpec::Visitor::visitStruct(const CTFASTStructSpec& structSpec)
{
	(void)structSpec;
	err_ts("Structure specification");
}

void CTFASTTypeSpec::Visitor::visitInt(const CTFASTIntSpec& intSpec)
{
	(void)intSpec;
	err_ts("Integer specification");
}
void CTFASTTypeSpec::Visitor::visitVariant
	(const CTFASTVariantSpec& variantSpec)
{
	(void)variantSpec;
	err_ts("Variant specification");
}
void CTFASTTypeSpec::Visitor::visitEnum(const CTFASTEnumSpec& enumSpec)
{
	(void)enumSpec;
	err_ts("Enumeration specification");
}
