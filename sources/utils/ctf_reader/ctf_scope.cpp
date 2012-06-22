#include "ctf_scope.h"
#include <algorithm>
#include <stdexcept>

#include <cstring> /* strchr() */

CTFScope::CTFScope(void) : parent(NULL)
{
}

static void deleteType(CTFType* type) {delete type;}
static void deleteScope(CTFScope* scope) {delete scope;}

CTFScope::~CTFScope(void)
{
	std::for_each(types.begin(), types.end(), deleteType);
	std::for_each(scopes.begin(), scopes.end(), deleteScope);
}

void CTFScope::addType(CTFType* type)
{
	types.push_back(type);
}

void CTFScope::addScope(CTFScope* scope)
{
	scopes.push_back(scope);
	scope->parent = this;
}

void CTFScope::addTypeName(const CTFType* type, const std::string& name)
{
	if(!typeNames.insert(name, type).second)
		throw std::logic_error("Type with name '" + name + "' already exists");
}

void CTFScope::addStructName(const CTFTypeStruct* typeStruct,
	const std::string& name)
{
	if(!structNames.insert(name, typeStruct).second)
		throw std::logic_error("Struct with name '" + name + "' already exists");
}

void CTFScope::addEnumName(const CTFTypeEnum* typeEnum,
	const std::string& name)
{
	if(!enumNames.insert(name, typeEnum).second)
		throw std::logic_error("Enum with name '" + name + "' already exists");
}

void CTFScope::addVariantName(const CTFTypeVariant* typeVariant,
	const std::string& name)
{
	if(!variantNames.insert(name, typeVariant).second)
		throw std::logic_error("Variant with name '" + name + "' already exists");
}


const CTFType* CTFScope::findTypeStrict(const std::string& name) const
{
	typeNames_t::const_iterator iter = typeNames.find(name);
	if(iter != typeNames.end())
	{
		return iter->second;
	}
	return NULL;
}

const CTFTypeStruct* CTFScope::findStructStrict
	(const std::string& name) const
{
	structNames_t::const_iterator iter = structNames.find(name);
	if(iter != structNames.end())
	{
		return iter->second;
	}
	return NULL;
}

const CTFTypeEnum* CTFScope::findEnumStrict
	(const std::string& name) const
{
	enumNames_t::const_iterator iter = enumNames.find(name);
	if(iter != enumNames.end())
	{
		return iter->second;
	}
	return NULL;
}

const CTFTypeVariant* CTFScope::findVariantStrict
	(const std::string& name) const
{
	variantNames_t::const_iterator iter = variantNames.find(name);
	if(iter != variantNames.end())
	{
		return iter->second;
	}
	return NULL;
}


const CTFType* CTFScope::findType(const std::string& name) const
{
	for(const CTFScope* scope = this; scope!= NULL; scope = scope->parent)
	{
		const CTFType* type = scope->findTypeStrict(name);
		if(type) return type;
	}
	return NULL;
}

const CTFTypeStruct* CTFScope::findStruct(const std::string& name) const
{
	for(const CTFScope* scope = this; scope!= NULL; scope = scope->parent)
	{
		const CTFTypeStruct* typeStruct = scope->findStructStrict(name);
		if(typeStruct) return typeStruct;
	}
	return NULL;
}

const CTFTypeEnum* CTFScope::findEnum(const std::string& name) const
{
	for(const CTFScope* scope = this; scope!= NULL; scope = scope->parent)
	{
		const CTFTypeEnum* typeEnum = scope->findEnumStrict(name);
		if(typeEnum) return typeEnum;
	}
	return NULL;
}

const CTFTypeVariant* CTFScope::findVariant(const std::string& name) const
{
	for(const CTFScope* scope = this; scope!= NULL; scope = scope->parent)
	{
		const CTFTypeVariant* typeVariant = scope->findVariantStrict(name);
		if(typeVariant) return typeVariant;
	}
	return NULL;
}


void CTFScopeTop::addParameter(const std::string& name, const std::string& value)
{
	if(!parameters.insert(name, value).second)
		throw std::logic_error("Parameter with name '" + name
			+ "' already exists");
}

const std::string* CTFScopeTop::findParameter(const std::string& name) const
{
	parameters_t::const_iterator iter = parameters.find(name);
	if(iter != parameters.end())
	{
		return &iter->second;
	}
	return NULL;
}

void CTFScopeRoot::addTopScopeName(const CTFScopeTop* scopeTop,
	const std::string& name)
{
	if(!scopesTop.insert(name, scopeTop).second)
		throw std::logic_error("Scope with name '" + name
			+ "' already exists");
}

const std::string* CTFScopeRoot::findParameter(const std::string& name) const
{
	size_t pointPos = name.find('.');
	if(pointPos == name.npos) return NULL;
	
	scopesTop_t::const_iterator iter
		= scopesTop.find(std::string(name, 0, pointPos));
	if(iter != scopesTop.end())
	{
		return iter->second->findParameter(std::string(name, pointPos + 1));
	}
	return NULL;
}