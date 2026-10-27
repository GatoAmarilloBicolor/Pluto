#ifndef MOCK_PROPERTY_INFO_H
#define MOCK_PROPERTY_INFO_H

#include "Types.h"

#define B_GET_PROPERTY 0
#define B_SET_PROPERTY 1
#define B_CREATE_PROPERTY 2
#define B_DELETE_PROPERTY 3
#define B_EXECUTE_PROPERTY 4
#define B_COUNT_PROPERTIES 5

#define B_DIRECT_SPECIFIER 0
#define B_INDEX_SPECIFIER 1
#define B_RANGE_SPECIFIER 2
#define B_REVERSE_INDEX_SPECIFIER 3
#define B_NAME_SPECIFIER 4

// Type codes are defined in Types.h

struct property_info {
	const char* name;
	uint32 commands[2];
	uint32 specifiers[2];
	const char* description;
	int32 usage;
	type_code types[4];
};

class BPropertyInfo {
public:
	BPropertyInfo(const property_info* properties) {
		(void)properties;
	}
	virtual ~BPropertyInfo() = default;
	
	virtual int32 FindMatch(BMessage* msg, int32 index, BMessage* specifier, 
	                        int32 what, const char* property) {
		(void)msg; (void)index; (void)specifier; (void)what; (void)property;
		return -1;
	}
	
	virtual status_t GetFlatSize(const property_info* properties, size_t* size) {
		(void)properties; (void)size;
		return B_OK;
	}
	
	virtual status_t Flatten(const property_info* properties, void* buffer, 
	                         size_t size) {
		(void)properties; (void)buffer; (void)size;
		return B_OK;
	}
};

#endif