#if ( !defined(ENUMS_H) || defined(GENERATE_ENUM_STRINGS) )

#if (!defined(GENERATE_ENUM_STRINGS))
    #define ENUMS_H
#endif

///////////////////////////////
// The enum examples
///////////////////////////////
//
// BEGIN_ENUM(Months)		/* enum2str with index value (starting at 2) */
//        ENUM_ELEMENT(january,=2,"jan")
//        ENUM_ELEMENT(february,,"feb")
//        ENUM_ELEMENT(march,,"mrch")
// END_ENUM(Months)
//
// /* will generate this in sccp_enum.h */
// typedef enum tag_Months {
//        january =2,
//        february ,
//        march ,
// } sccp_Months_type_t; 
//
// inline const char* Months2str(sccp_Months_type_t index);
//
// /* and this in sccp_enum.c */
// inline const char* Months2str(sccp_Months_type_t index) { 
// 	switch(index) { 
// 		default: printf("ERROR: element %d could not be found in sccp_" "Months" "_type_t", index); return "ERROR";
//         	case january: return "jan";
//         	case february: return "feb";
//         	case march: return "mrch";
// 	};
// };

/* enum macro definitions */
#undef ENUM_ELEMENT
#undef BEGIN_ENUM
#undef END_ENUM

#ifndef GENERATE_ENUM_STRINGS												// header definition (1st pass)
    #define ENUM_ELEMENT( element, index, str) element index,
    #define BEGIN_ENUM( ENUM_NAME ) typedef enum tag_##ENUM_NAME {
    #define END_ENUM( ENUM_NAME ) } sccp_##ENUM_NAME##_type_t; 								\
        inline const char* ENUM_NAME##2str(sccp_##ENUM_NAME##_type_t index);
#else															// class definition (2nd pass)
#  ifdef DEBUG														// switch/case on index (Debug mode)
    #define ENUM_ELEMENT( element, index, str) case element: return str;
    #define BEGIN_ENUM( ENUM_NAME ) 											\
        inline const char* ENUM_NAME##2str(sccp_##ENUM_NAME##_type_t index) { 						\
                switch(index) {												\
                        default:											\
                                printf("ERROR: element %d could not be found in sccp_" #ENUM_NAME "_type_t", index); 	\
                                return "ERROR";									
    #define END_ENUM( ENUM_NAME ) 	};										\
        };
#  else 														// return array[index] directly (Non-Debug Mode)
    #define ENUM_ELEMENT( element, index, str ) [element] = str,
    #define BEGIN_ENUM( ENUM_NAME ) char* sccp_##ENUM_NAME##_map_enum2str[] = {
    #define END_ENUM( ENUM_NAME ) }; 											\
        inline const char* ENUM_NAME##2str(sccp_##ENUM_NAME##_type_t index) { return sccp_##ENUM_NAME##_map_enum2str[index]; }
#  endif
#endif


///////////////////////////////
// The actual enum declarations
///////////////////////////////
/*!
 * \brief SCCP Event Type ENUM
 */

BEGIN_ENUM(event)
        ENUM_ELEMENT(SCCP_EVENT_LINE_CREATED,		=1<<0,	"Line Created")
        ENUM_ELEMENT(SCCP_EVENT_LINE_CHANGED,		=1<<1,	"Line Changed")
        ENUM_ELEMENT(SCCP_EVENT_LINE_DELETED,		=1<<2,	"Line Deleted")
        ENUM_ELEMENT(SCCP_EVENT_DEVICE_ATTACHED,	=1<<3,	"Device Attached")
        ENUM_ELEMENT(SCCP_EVENT_DEVICE_DETACHED,	=1<<4,	"Device Detached")
        ENUM_ELEMENT(SCCP_EVENT_DEVICE_PREREGISTERED,	=1<<5,	"Device Preregistered")
        ENUM_ELEMENT(SCCP_EVENT_DEVICE_REGISTERED,	=1<<6,	"Device Registered")
        ENUM_ELEMENT(SCCP_EVENT_DEVICE_UNREGISTERED,	=1<<7,	"Device Unregistered")
        ENUM_ELEMENT(SCCP_EVENT_FEATURE_CHANGED,	=1<<8,	"Feature Changed")
        ENUM_ELEMENT(SCCP_EVENT_LINESTATUS_CHANGED,	=1<<9,	"LineStatus Changed")
END_ENUM(event)

#endif // (!defined(ENUMS_H) || defined(GENERATE_ENUM_STRINGS))
