/*
 * Scope - abstract essence which store types, allows to search them
 * and perform some other operations.
 */

#ifndef CTF_SCOPE_H_INCLUDED
#define CTF_SCOPE_H_INCLUDED

#include <string>

class CTFType;

class CTFScope
{
public:
	/* Create scope with given parent(NULL for root scope). */
	CTFScope(const CTFScope* parent);
	
	virtual ~CTFScope(void);
	
	/*
	 * Search type with given name in scope and all of its parents.
	 */
	const CTFType* findType(std::string name) const;

	/*
	 * Search type with given name only in given scope.
	 */
	const CTFType* findTypeStrict(std::string name) const
		{return findTypeImpl(name);}

	/*
	 * Find parameter in the scope.
	 * 
	 * Return NULL pointer if not found.
	 */
	const std::string* findParameter(std::string paramName) const
		{return findParameterImpl(paramName);}

	/* 
	 * If scope connected to some type, return this type.
	 * 
	 * Needed for resolving tags.
	 */
	const CTFType* getTypeConnected(void) const
		{return getTypeConnectedImpl(); }
protected:
	virtual const CTFType* findTypeImpl(std::string name) const
		{(void)name; return NULL;}
	virtual const std::string* findParameterImpl
		(std::string paramName) const
		{ (void)paramName; return NULL;}
	
	virtual const CTFType* getTypeConnectedImpl(void) const
		{return NULL;}
private:
	const CTFScope* parent;
};

#endif /* CTF_SCOPE_H_INCLUDED */