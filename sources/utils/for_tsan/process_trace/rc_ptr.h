/* rc_ptr.h
 * Definition of a helper reference counter smart pointer class. */
#ifndef __RCPTR_H__72467B98_4225_41B4_A932_F282C93CB033_INCLUDED_
#define __RCPTR_H__72467B98_4225_41B4_A932_F282C93CB033_INCLUDED_

/* rc_ptr<T>
 * Represents a reference counter smart pointer. The object it points to
 * is automatically deleted when the last rc_ptr referring to it is 
 * destroyed. */
template <class T>
class rc_ptr 
{
	T *ptr; /* the pointer to the value */
	unsigned long *count; /* number of references (shared) */

public:
	/* Ctor. Initializes the pointer with an existing pointer.
	 * [NB] It requires that the pointer "p" is a return value of 
	 * new. */
	explicit rc_ptr(T *p = NULL)
		: ptr(p), count(new unsigned long(1)) 
	{}

	/* Copy ctor. Copy the pointer (one more owner appears). */
	rc_ptr(const rc_ptr<T> &p) throw()
		: ptr(p.ptr), count(p.count) 
	{
		++*count;
	}

	/* Dtor, deletes the  value if this was the last owner. */
	~rc_ptr() throw() 
	{
		dispose();
	}

	/* Assignment ("unshare" the old value and share the new one). */
	rc_ptr<T> & operator=(const rc_ptr<T> &p) throw() 
	{
		if (this != &p) 
		{
			dispose();
			ptr = p.ptr;
			count = p.count;
			++*count;
		}
		return *this;
	}

	/* Access the value the pointer refers to. */
	T& operator*() const throw() 
	{
		return *ptr;
	}
	T* operator->() const throw() 
	{
		return ptr;
	}

	int isNULL()
	{
		return (ptr == NULL);
	}
	
	unsigned long ref_count() const
	{
		return *count;
	}
	
private:
	void dispose() 
	{
		if (--*count == 0) {
			delete count;
			delete ptr;
		}
	}
};

#endif /* __RCPTR_H__72467B98_4225_41B4_A932_F282C93CB033_INCLUDED_ */
