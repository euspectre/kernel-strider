#include <kedr/ctf_reader/ctf_reader.h>

#include <stdexcept>
#include <cstring>
#include <cassert>
#include <iostream>

#include <endian.h>

#include <sstream>

#include <list>

#include <memory> /* auto_ptr*/

static int test_int(void);
static int test_struct(void);
static int test_enum(void);
static int test_float_offset(void);
static int test_variant(void);
static int test_array(void);
static int test_sequence(void);

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	int result;

#define RUN_TEST(test_func, test_name) do {\
    try {result = test_func(); }\
	catch(std::exception& e) \
	{ \
		std::cerr << "Exception occures in '" << test_name << "': " \
			<< e.what() << "." << std::endl; \
		return 1; \
    } \
    if(result) return result; \
}while(0)

	RUN_TEST(test_int, "Integer variable test");
	RUN_TEST(test_struct, "Structure variable test");
	RUN_TEST(test_enum, "Enumeration variable test");
	RUN_TEST(test_float_offset, "Variables with floating offset test");
	RUN_TEST(test_variant, "Variant variable test");
	RUN_TEST(test_array, "Array variable test");
	RUN_TEST(test_sequence, "Sequence variable test");

	return 0;
}


/* Simply make all protected interfaces public */
class CTFMetaTest: public CTFMeta
{
public:
    const CTFVar* instantiate(const CTFType* rootType)
        {return CTFMeta::instantiate(rootType); }

    /* Factory for types */
    CTFTypeInt* createTypeInt(void) const
        {return CTFMeta::createTypeInt();}
    CTFTypeStruct* createTypeStruct(void) const
        {return CTFMeta::createTypeStruct();}
    CTFTypeEnum* createTypeEnum(CTFTypeInt* baseTypeInt) const
        {return CTFMeta::createTypeEnum(baseTypeInt);}
    CTFTypeVariant* createTypeVariant(void) const
        {return CTFMeta::createTypeVariant();}
    CTFTypeArray* createTypeArray(int size, CTFType* elemType) const
        {return CTFMeta::createTypeArray(size, elemType);}
    CTFTypeSequence* createTypeSequence(CTFTag tagNElems, CTFType* elemType) const
        {return CTFMeta::createTypeSequence(tagNElems, elemType);}
};

#define NOT_ACCESSIBLE(what) throw std::logic_error(std::string(what) + \
": shouldn't be accessed.")

static char messageComponent[] = "message";

/* Simple root type - wraps single type.*/

class MessageType : public CTFType
{
public:
	MessageType(const CTFType* type) : type(type) {}
	const CTFType* getType(void) const {return type;}
protected:
    CTFType* cloneImpl(void) const
		{NOT_ACCESSIBLE("cloneImpl");}
    int getAlignmentImpl(void) const
		{NOT_ACCESSIBLE("getAlignmentImpl");}
    int getAlignmentMaxImpl(void) const
		{return type->getAlignmentMax();}
    void setVarImpl(CTFVarPlace& varPlace) const;

    CTFTag resolveTagImpl(const char* tagStr,
        const char** tagStrEnd) const;
    CTFTag resolveTagContinueImpl(const char* tagStr,
        const char** tagStrEnd) const
        {(void) tagStr; (void)tagStrEnd; NOT_ACCESSIBLE("resolveTagContinueImpl");}
private:
	const CTFType* type;
};

class MessageVarPlace;

class MessageVar: public CTFVar
{
public:
	MessageVar(const MessageType* messageType)
		: messageType(messageType), messageVarPlace(NULL) {}
	~MessageVar();
	void instantiateChild(void);
	const CTFVarPlaceContext* getContextVar(void) const;

protected:
    int getAlignmentImpl(CTFContext& context) const
		{(void) context; NOT_ACCESSIBLE("getAlignmentImpl");}
    int getAlignmentImpl(void) const
		{NOT_ACCESSIBLE("getAlignmentImpl");}
    int getStartOffsetImpl(CTFContext& context) const
		{(void) context; NOT_ACCESSIBLE("getStartOffsetImpl");}
    int getStartOffsetImpl(void) const
		{NOT_ACCESSIBLE("getStartOffsetImpl");}
    int getEndOffsetImpl(CTFContext& context) const
		{(void) context; NOT_ACCESSIBLE("getEndOffsetImpl");}
    int getEndOffsetImpl(void) const
		{NOT_ACCESSIBLE("getEndOffsetImpl");}
    int getSizeImpl(CTFContext& context) const
		{(void) context; NOT_ACCESSIBLE("getSizeImpl");}
    int getSizeImpl(void) const
		{NOT_ACCESSIBLE("getSizeImpl");}

    const CTFVar* resolveNameImpl(const char* name,
        const char** nameEnd, bool isContinued) const;

    const CTFType* getTypeImpl(void) const {return messageType;}

    const MessageType* messageType;
	MessageVarPlace* messageVarPlace;
};

class MessageVarPlace: public CTFVarPlaceContext
{
public:
	MessageVarPlace(const MessageVar* messageVar)
		: messageVar(messageVar) {}

	const CTFVar* getParentVar(void) const {return messageVar;}
protected:
	std::string getNameImpl(void) const {return "message";}
private:
	const MessageVar* messageVar;
};

void MessageVar::instantiateChild(void)
{
	messageVarPlace = new MessageVarPlace(this);

	messageVarPlace->instantiateVar(messageType->getType());
}

const CTFVarPlaceContext* MessageVar::getContextVar(void) const
{
	return messageVarPlace;
}

MessageVar::~MessageVar()
{
	if(messageVarPlace) delete messageVarPlace;
}

const CTFVar* MessageVar::resolveNameImpl(const char* name,
    const char** nameEnd, bool isContinued) const
{
	(void)isContinued;
	if(strncmp(name, messageComponent, sizeof(messageComponent) - 1) != 0)
		return NULL;
	*nameEnd = name + sizeof(messageComponent) - 1;
	return messageVarPlace->getVar();
}

void MessageType::setVarImpl(CTFVarPlace& varPlace) const
{
	MessageVar* messageVar = new MessageVar(this);
	varPlace.setVar(messageVar);
	messageVar->instantiateChild();
}

CTFTag MessageType::resolveTagImpl(const char* tagStr,
	const char** tagStrEnd) const
{
	if(strncmp(tagStr, messageComponent, sizeof(messageComponent) - 1) != 0)
		return CTFTag();
	*tagStrEnd = tagStr + sizeof(messageComponent) - 1;
	return CTFTag(this, messageComponent, type);
}


/* 
 * Context which maps memory area with constant size and constant address.
 */
class StaticContext : public CTFContext
{
public:
	/* 
	 * Note, that buffer is copied.
	 * 
	 * This needed for satisfy alignment of the context mapping.
	 */
	StaticContext(const CTFVarPlaceContext *contextVar,
		const char* buffer, int size) : CTFContext(contextVar)
	{
		this->buffer = new char[size];
		memcpy(this->buffer, buffer, size);
		
		moveMap(size * 8, this->buffer, 0);
	}
	
	~StaticContext(void) {delete[] buffer;}
	/* 
	 * Return map for the context.
	 * 
	 * This value is garantee satisfy needed alignment, in contrast to
	 * 'buffer' parameter of the constructor.
	 */
	const char* getMap(void) const{return buffer;}
protected:
	int extendMapImpl(int newSize, const char** mapStart_p,
		int* mapStartShift_p)
	{
		(void)newSize;
		(void)mapStart_p;
		(void)mapStartShift_p;
		
		std::cerr << "Static context cannot be extended. " 
			<< "(Requested its extension to " << newSize << ".)";
		
		throw std::logic_error("Cannot extend static context");
	}
private:
	char* buffer;
};

/* Test integer type and its variable */
int test_int()
{
	CTFMetaTest meta;

	std::auto_ptr<CTFTypeInt> typeInt(meta.createTypeInt());

	typeInt->setSize(32);
	typeInt->setAlignment(32);
	typeInt->setByteOrder(CTFTypeInt::be);
	typeInt->setSigned(1);
	typeInt->fixParams();

	MessageType rootType(typeInt.get());

	const MessageVar* messageVar = static_cast<const MessageVar*>
		(meta.instantiate(&rootType));

	const CTFVarInt* var = static_cast<const CTFVarInt*>
		(meta.findVar("message"));
	assert(var != NULL);

	char buf[4] = {'1', '2', '3', '4'};
	StaticContext staticContext(messageVar->getContextVar(), buf, 4);

	int val = var->getInt32(staticContext);
	int val_expected = be32toh(*(const int32_t*)staticContext.getMap());

	if(val != val_expected)
	{
		std::cerr << "Expected, that value of variable will be "
			<< val_expected << ", but it is " << val << "." << std::endl;
		return 1;
	}
	return 0;
}

/* Test structure type and its variable. */
int test_struct()
{
	CTFMetaTest meta;

	std::auto_ptr<CTFTypeInt> typeInt1(meta.createTypeInt());

	typeInt1->setSize(16);
	typeInt1->setAlignment(16);
	typeInt1->setByteOrder(CTFTypeInt::le);
	typeInt1->setSigned(0);
	typeInt1->fixParams();

	std::auto_ptr<CTFTypeInt> typeInt2(meta.createTypeInt());

	typeInt2->setSize(32);
	typeInt2->setAlignment(32);
	typeInt2->setByteOrder(CTFTypeInt::be);
	typeInt2->setSigned(1);
	typeInt2->fixParams();

	std::auto_ptr<CTFTypeStruct> typeStruct(meta.createTypeStruct());

	typeStruct->addField("field1", typeInt1.get());
	typeStruct->addField("field2", typeInt2.get());

	MessageType rootType(typeStruct.get());

	const MessageVar* messageVar = static_cast<const MessageVar*>
		(meta.instantiate(&rootType));

	char buf[8] = {'1', '2', '3', '4', '5', '6', '7', '8'};
	StaticContext staticContext(messageVar->getContextVar(), buf, 8);

	const CTFVarInt* var2 = static_cast<const CTFVarInt*>
		(meta.findVar("message.field2"));
	if(var2 == NULL)
	{
		std::cerr << "Failed to find variable corresponded to the "
			"second field of the structure." << std::endl;
		return 1;
	}

	int val2 = var2->getInt32(staticContext);
	int val2_expected = be32toh(*(const int32_t*)(staticContext.getMap() + 4));

	if(val2 != val2_expected)
	{
		std::cerr << "Expected, that value of second field will be "
			<< val2_expected << ", but it is " << val2 << "." << std::endl;
		return 1;
	}
	return 0;
}

/* Test enumeration type and its variable */
int test_enum()
{
	CTFMetaTest meta;

	std::auto_ptr<CTFTypeInt> typeInt(meta.createTypeInt());

	typeInt->setSize(32);
	typeInt->setAlignment(32);
	typeInt->setByteOrder(CTFTypeInt::be);
	typeInt->setSigned(1);
	typeInt->fixParams();

	std::auto_ptr<CTFTypeEnum> typeEnum(meta.createTypeEnum(typeInt.get()));

	typeEnum->addValue32("One", 1, 1);
	typeEnum->addValue64("Three", 3, 3);
	typeEnum->addValue32("More", 5, 7);
	typeEnum->addValue32("EvenMore", 8, 11);


	MessageType rootType(typeEnum.get());

	const MessageVar* messageVar = static_cast<const MessageVar*>
		(meta.instantiate(&rootType));

	const CTFVarEnum* var = static_cast<const CTFVarEnum*>
		(meta.findVar("message"));
	assert(var != NULL);

	char buf[4] = {'\0', '\0', '\0', '\007'};
	StaticContext staticContext(messageVar->getContextVar(), buf, 4);

	std::string enumVal = var->getEnum(staticContext);
	assert(enumVal != "");
	std::string enumVal_expected = "More";

	if(enumVal != enumVal_expected)
	{
		std::cerr << "Expected, that value of enumeration will be '"
			<< enumVal_expected << "', but it is '" << enumVal << "'." << std::endl;
		return 1;
	}
	return 0;
}

/* Type which create flexible variable. */
class FlexType : public CTFType
{
protected:
	CTFType* cloneImpl() const {return new FlexType();}
	int getAlignmentMaxImpl(void) const {return 1;}

	void setVarImpl(CTFVarPlace& varPlace) const;
};
/*
 * Variable which changes its size depended on context.
 *
 * Should be first in the context.
 */
class FlexVar : public CTFVar
{
public:
	FlexVar(const FlexType* flexType,
		const CTFVar* container, const CTFVar* prev)
		: flexType(flexType), container(container), prev(prev) {}

	/* Force variable to have given size in given context. */
	void setSize(int size, CTFContext& context) const
	{
		CTFContext* contextAdjusted = adjustContext(context);
        assert(contextAdjusted);
        
        *contextAdjusted->getCache(sizeElemIndex) = size;
	}
protected:
    void onPlaceChanged(const CTFVarPlace* oldPlace)
    {
        if(oldPlace)
        {
            oldPlace->getContextVar()->cancelCacheReservation(sizeElemIndex);
        }
        const CTFVarPlace* varPlace = getVarPlace();
        if(varPlace)
        {
            sizeElemIndex = varPlace->getContextVar()->reserveCache();
        }
    }

    int getAlignmentImpl(CTFContext& context) const
		{(void) context; return 1;}
    int getAlignmentImpl(void) const {return 1;}
    int getSizeImpl(CTFContext& context) const
    {
		CTFContext* contextAdjusted = adjustContext(context);
		if(contextAdjusted == NULL) return -1;
        int size = *contextAdjusted->getCache(sizeElemIndex);
        assert(size != -1);

		return size;
    }
    int getSizeImpl(void) const {return -1;}

	/*
	 * Very simple implementation of start offset getter.
	 * In real world, should be one derived class for each layout type.
	 */
	int getStartOffsetImpl(CTFContext& context) const
	{
		if(prev) return prev->getEndOffset(context);
		else if(container) return container->getStartOffset(context);
		else return 0;
	}
	int getStartOffsetImpl(void) const
	{
		if(prev) return prev->getEndOffset();
		else if(container) return container->getStartOffset();
		else return 0;
	}


	int getEndOffsetImpl(CTFContext& context) const
	{
		CTFContext* contextAdjusted = adjustContext(context);
		if(contextAdjusted == NULL) return -1;
        int size = *contextAdjusted->getCache(sizeElemIndex);
        assert(size != -1);

		int startOffset;

		if(prev) startOffset = prev->getEndOffset(context);
		else if(container) startOffset =  container->getStartOffset(context);
		else startOffset = 0;

		return startOffset + size;
	}
	int getEndOffsetImpl(void) const {return -1;}

    const CTFType* getTypeImpl(void) const {return flexType;}
private:
	const FlexType* flexType;

	const CTFVar* container;
	const CTFVar* prev;
    
    int sizeElemIndex;
};

void FlexType::setVarImpl(CTFVarPlace& varPlace) const
{
	varPlace.setVar(new FlexVar(this, varPlace.getContainerVar(),
        varPlace.getPreviousVar()));
}

/* Test layout with floating offsets of variables */
int test_float_offset(void)
{
	CTFMetaTest meta;

	FlexType typeFlex;

	std::auto_ptr<CTFTypeInt> typeInt1(meta.createTypeInt());

	typeInt1->setSize(16);
	typeInt1->setAlignment(16);
	typeInt1->setByteOrder(CTFTypeInt::le);
	typeInt1->setSigned(0);
	typeInt1->fixParams();

	std::auto_ptr<CTFTypeInt> typeInt2(meta.createTypeInt());

	typeInt2->setSize(32);
	typeInt2->setAlignment(32);
	typeInt2->setByteOrder(CTFTypeInt::be);
	typeInt2->setSigned(1);
	typeInt2->fixParams();

	std::auto_ptr<CTFTypeStruct> typeStruct(meta.createTypeStruct());

	typeStruct->addField("flex", &typeFlex);
	typeStruct->addField("field1", typeInt1.get());
	typeStruct->addField("field2", typeInt2.get());

	MessageType rootType(typeStruct.get());

	const MessageVar* messageVar = static_cast<const MessageVar*>
		(meta.instantiate(&rootType));

	char buf[8] = {'1', '2', '3', '4', '5', '6', '7', '8'};
	StaticContext staticContext(messageVar->getContextVar(), buf, 8);

	const FlexVar* flexVar = static_cast<const FlexVar*>
		(meta.findVar("message.flex"));
	flexVar->setSize(0, staticContext);

	const CTFVarInt* var2 = static_cast<const CTFVarInt*>
		(meta.findVar("message.field2"));
	if(var2 == NULL)
	{
		std::cerr << "Failed to find variable corresponded to the "
			"second field of the structure." << std::endl;
		return 1;
	}

	int val2 = var2->getInt32(staticContext);
	int val2_expected = be32toh(*(const int32_t*)(staticContext.getMap() + 4));

	if(val2 != val2_expected)
	{
		std::cerr << "Expected, that value of second field will be "
			<< val2_expected << ", but it is " << val2 << ".\n";
		return 1;
	}


	char buf1[12] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', 'a',
		'b', 'c'};
	StaticContext staticContext1(messageVar->getContextVar(),
		buf1, sizeof(buf1));

	flexVar->setSize(3*8, staticContext1);

	val2 = var2->getInt32(staticContext1);
	val2_expected = be32toh(*(const int32_t*)(staticContext1.getMap() + 8));

	if(val2 != val2_expected)
	{
		std::cerr << "Expected, that value of second field in new context will be "
			<< val2_expected << ", but it is " << val2 << "." << std::endl;
		return 1;
	}

	return 0;
}

/* Test variant type and its variable */
int test_variant()
{
	CTFMetaTest meta;

	std::auto_ptr<CTFTypeInt> typeInt16(meta.createTypeInt());

	typeInt16->setSize(16);
	typeInt16->setAlignment(16);
	typeInt16->setByteOrder(CTFTypeInt::be);
	typeInt16->setSigned(1);
	typeInt16->fixParams();

	std::auto_ptr<CTFTypeInt> typeInt32(meta.createTypeInt());

	typeInt32->setSize(32);
	typeInt32->setAlignment(32);
	typeInt32->setByteOrder(CTFTypeInt::be);
	typeInt32->setSigned(1);
	typeInt32->fixParams();

	std::auto_ptr<CTFTypeEnum> typeEnum(meta.createTypeEnum(typeInt16.get()));

	typeEnum->addValue32("One", 1, 1);
	typeEnum->addValue64("Three", 3, 3);
	typeEnum->addValue32("More", 5, 7);
	typeEnum->addValue32("EvenMore", 8, 11);

	std::auto_ptr<CTFTypeStruct> typeStruct(meta.createTypeStruct());

	typeStruct->addField("selector", typeEnum.get());

	CTFTag selectorTag(typeStruct.get(), "selector", typeEnum.get());

	std::auto_ptr<CTFTypeVariant> typeVariant(meta.createTypeVariant());

	typeVariant->setTag(selectorTag);

	typeVariant->addField("EvenMore", typeInt32.get());
	typeVariant->addField("More", typeInt16.get());

	typeStruct->addField("info", typeVariant.get());

	MessageType rootType(typeStruct.get());

	const MessageVar* messageVar = static_cast<const MessageVar*>
		(meta.instantiate(&rootType));

	const CTFVarInt* varMore = static_cast<const CTFVarInt*>
		(meta.findVar("message.info.More"));
	assert(varMore != NULL);
	const CTFVarInt* varEvenMore = static_cast<const CTFVarInt*>
		(meta.findVar("message.info.EvenMore"));
	assert(varEvenMore != NULL);


	char buf[8] = {'\0', '\012', '?', '?', '1', '2', '3', '4'};
	StaticContext staticContext(messageVar->getContextVar(), buf, 8);

	if(varEvenMore->isExist(staticContext) != 1)
	{
		std::cerr << "Variant field with name 'EvenMore' should exist in "
			"the context under test." << std::endl;
		return 1;
	}

	if(varMore->isExist(staticContext) != 0)
	{
		std::cerr << "Variant field with name 'More' shouldn't exist in "
			"the context under test." << std::endl;
		return 1;
	}


	int val = varEvenMore->getInt32(staticContext);
	int val_expected = be32toh(*(const int32_t*)(staticContext.getMap() + 4));

	if(val != val_expected)
	{
		std::cerr << "Expected, that value of variant field will be "
			<< val_expected << ", but it is " << val << "." << std::endl;
		return 1;
	}
	return 0;
}

/* Test array type and its variable */
int test_array()
{
	CTFMetaTest meta;

	std::auto_ptr<CTFTypeInt> typeInt16(meta.createTypeInt());

	typeInt16->setSize(16);
	typeInt16->setAlignment(16);
	typeInt16->setByteOrder(CTFTypeInt::be);
	typeInt16->setSigned(1);
	typeInt16->fixParams();

	std::auto_ptr<CTFTypeInt> typeInt8(meta.createTypeInt());

	typeInt8->setSize(8);
	typeInt8->setAlignment(8);
	typeInt8->setByteOrder(CTFTypeInt::be);
	typeInt8->setSigned(1);
	typeInt8->fixParams();

	std::auto_ptr<CTFTypeArray> typeArray(meta.createTypeArray(7,
		typeInt8.get()));

	std::auto_ptr<CTFTypeStruct> typeStruct(meta.createTypeStruct());

	typeStruct->addField("field_first", typeInt16.get());
	typeStruct->addField("array", typeArray.get());
	typeStruct->addField("field_last", typeInt16.get());

	MessageType rootType(typeStruct.get());

	const MessageVar* messageVar = static_cast<const MessageVar*>
		(meta.instantiate(&rootType));

	const CTFVarArray* varArray = static_cast<const CTFVarArray*>
		(meta.findVar("message.array"));
	assert(varArray != NULL);

	const CTFVarInt* varElem = static_cast<const CTFVarInt*>
		(meta.findVar("message.array[]"));
	assert(varElem != NULL);
	const CTFVarInt* varLast = static_cast<const CTFVarInt*>
		(meta.findVar("message.field_last"));
	assert(varLast != NULL);


	char buf[12] = {'1', '2', '1', '2', '3', '4', '5', '6', '7', '?', '3', '4'};
	StaticContext staticContext(messageVar->getContextVar(), buf, 12);

	int i = 0;
	for(CTFVarArray::ElemIterator iter(*varArray, staticContext);
		iter;
		++iter, ++i)
	{
		int val = varElem->getInt32(*iter);
		int val_expected = staticContext.getMap()[2 + i];

		if(val != val_expected)
		{
			std::cerr << "Expected, that value of " << i << " array element "
				"will be " << val_expected << ", but it is " << val << "." << std::endl;
			return 1;
		}

	}

	int val = varLast->getInt32(staticContext);
	int val_expected = be16toh(*(const int16_t*)(staticContext.getMap() + 10));

	if(val != val_expected)
	{
		std::cerr << "Expected, that value of variable after array will be "
			<< val_expected << ", but it is " << val << "." << std::endl;
		return 1;
	}
	return 0;
}

/* Test sequence type and its variable */
int test_sequence()
{
	CTFMetaTest meta;

	std::auto_ptr<CTFTypeInt> typeInt16(meta.createTypeInt());

	typeInt16->setSize(16);
	typeInt16->setAlignment(16);
	typeInt16->setByteOrder(CTFTypeInt::be);
	typeInt16->setSigned(1);
	typeInt16->fixParams();

	std::auto_ptr<CTFTypeInt> typeInt8(meta.createTypeInt());

	typeInt8->setSize(8);
	typeInt8->setAlignment(8);
	typeInt8->setByteOrder(CTFTypeInt::be);
	typeInt8->setSigned(1);
	typeInt8->fixParams();




	std::auto_ptr<CTFTypeStruct> typeStruct(meta.createTypeStruct());

	typeStruct->addField("size", typeInt16.get());

	CTFTag tagNElems(typeStruct.get(), "size", typeInt16.get());

	std::auto_ptr<CTFTypeSequence> typeSequence(meta.createTypeSequence
		(tagNElems, typeInt8.get()));


	typeStruct->addField("sequence", typeSequence.get());
	typeStruct->addField("field_last", typeInt16.get());

	MessageType rootType(typeStruct.get());

	const MessageVar* messageVar = static_cast<const MessageVar*>
		(meta.instantiate(&rootType));

	const CTFVarArray* varSequence = static_cast<const CTFVarArray*>
		(meta.findVar("message.sequence"));
	assert(varSequence != NULL);

	const CTFVarInt* varElem = static_cast<const CTFVarInt*>
		(meta.findVar("message.sequence[]"));
	assert(varElem != NULL);
	const CTFVarInt* varLast = static_cast<const CTFVarInt*>
		(meta.findVar("message.field_last"));
	assert(varLast != NULL);


	char buf[10] = {'\0', '\005', '1', '2', '3', '4', '5', '?', '3', '4'};
	StaticContext staticContext(messageVar->getContextVar(), buf, 10);

	int nElems = varSequence->getNElems(staticContext);
	int nElems_expected = 5;

	if(nElems != nElems_expected)
	{
		std::cerr << "Expected, that number of element in sequence will be "
			<< nElems_expected << ", but it is " << nElems << "." << std::endl;
		return 1;
	}

	int i = 0;
	for(CTFVarArray::ElemIterator iter(*varSequence, staticContext);
		iter;
		++iter, ++i)
	{
		int val = varElem->getInt32(*iter);
		int val_expected = staticContext.getMap()[2 + i];

		if(val != val_expected)
		{
			std::cerr << "Expected, that value of " << i << " sequence element "
				"will be " << val_expected << ", but it is " << val << "." << std::endl;
			return 1;
		}
	}

	int val = varLast->getInt32(staticContext);
	int val_expected = be16toh(*(const int16_t*)(staticContext.getMap() + 8));

	if(val != val_expected)
	{
		std::cerr << "Expected, that value of variable after sequence will be "
			<< val_expected << ", but it is " << val << "." << std::endl;
		return 1;
	}
	return 0;
}
