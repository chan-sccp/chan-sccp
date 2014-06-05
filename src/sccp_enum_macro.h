#undef BEGIN_ENUM
#undef ENUM_ELEMENT
#undef END_ENUM
/*
 * GENERATE HEADER DECLARATIONS AND FUNCTION PROTOTYPES
 */
#if ENUMMACRO_GENERATE == 1
/*
 * FASE 1: Generate Enum + Function Prototypes
 */
#define BEGIN_ENUM(NAMESPACE, ENUM_NAME) typedef enum {
#define ENUM_ELEMENT(element, index, str) 	element index,
#define END_ENUM(NAMESPACE, ENUM_NAME) NAMESPACE##_##ENUM_NAME##_LOOKUPERROR =-1									\
} NAMESPACE##_##ENUM_NAME##_t;																\
const char * NAMESPACE##_##ENUM_NAME##2str(NAMESPACE##_##ENUM_NAME##_t value);										\
NAMESPACE##_##ENUM_NAME##_t NAMESPACE##_##ENUM_NAME##_str2val(const char *lookup_str);

#define BEGIN_LONGENUM BEGIN_ENUM
#define LONGENUM_ELEMENT(element, index, str, longstr) ENUM_ELEMENT(element, index, str)
#define END_LONGENUM(NAMESPACE, ENUM_NAME) END_ENUM(NAMESPACE, ENUM_NAME)										\
const char * NAMESPACE##_##ENUM_NAME##2longstr(NAMESPACE##_##ENUM_NAME##_t value);									\
NAMESPACE##_##ENUM_NAME##_t NAMESPACE##_##ENUM_NAME##_longstr2val(const char *lookup_longstr);

#include ENUMMACRO_FILE
#endif
#undef BEGIN_ENUM
#undef ENUM_ELEMENT
#undef END_ENUM
//#undef BEGIN_LONGENUM
//#undef LONGENUM_ELEMENT
//#undef END_LONGENUM
/*
 * GENERATE FUNCTION IMPLEMENTATIONS
 */
#if ENUMMACRO_GENERATE == 2

/*
 * FASE 1: Generate Map
 */
#define BEGIN_ENUM(NAMESPACE, ENUM_NAME) static const struct {												\
	const char *str;																\
} NAMESPACE##_##ENUM_NAME##_map[] = {
#define ENUM_ELEMENT(element, index, str) [element] = {str},
#define END_ENUM(NAMESPACE, ENUM_NAME) };

#define BEGIN_LONGENUM(NAMESPACE, ENUM_NAME) static const struct {											\
	const char *str;																\
	const char *longstr;																\
} NAMESPACE##_##ENUM_NAME##_map[] = {
#define LONGENUM_ELEMENT(element, index, str, longstr) [element] = {str, longstr},
#define END_LONGENUM(NAMESPACE, ENUM_NAME) };

#include ENUMMACRO_FILE
#undef BEGIN_ENUM
#undef ENUM_ELEMENT
#undef END_ENUM
#undef BEGIN_LONGENUM
#undef LONGENUM_ELEMENT
#undef END_LONGENUM

/*
 * FASE 2: Generate Exists Array (To lookup index position for sparse enums)
 *         Generate Lookup Functions
 */
#define BEGIN_ENUM(NAMESPACE, ENUM_NAME) static const int NAMESPACE##_##ENUM_NAME##_exists[] = {
#define ENUM_ELEMENT(element, index, str) element,
#define END_ENUM(NAMESPACE, ENUM_NAME) };														\
const char * NAMESPACE##_##ENUM_NAME##2str(NAMESPACE##_##ENUM_NAME##_t value) {										\
	if (value >= NAMESPACE##_##ENUM_NAME##_exists[0] && value <= NAMESPACE##_##ENUM_NAME##_exists[ARRAY_LEN(NAMESPACE##_##ENUM_NAME##_exists)-1]) {	\
		return NAMESPACE##_##ENUM_NAME##_map[value].str;											\
	} else {																	\
		pbx_log(LOG_NOTICE, "SCCP: Error during lookup of '%d' in " #NAMESPACE "_" #ENUM_NAME "_val2str\n", value);				\
		return "SCCP: OutOfBounds Error during lookup of " #NAMESPACE "_" #ENUM_NAME "\n";											\
	}																		\
}																			\
NAMESPACE##_##ENUM_NAME##_t NAMESPACE##_##ENUM_NAME##_str2val(const char * lookup_str) {								\
	int idx;																	\
	for(idx=0; idx<=ARRAY_LEN(NAMESPACE##_##ENUM_NAME##_exists) - 1; idx++) {										\
		if (!strcasecmp(NAMESPACE##_##ENUM_NAME##_map[NAMESPACE##_##ENUM_NAME##_exists[idx]].str, lookup_str)) {				\
			return NAMESPACE##_##ENUM_NAME##_exists[idx];											\
		}																	\
	}																		\
	pbx_log(LOG_NOTICE, "SCCP: Error during lookup of '%s' in " #NAMESPACE "_" #ENUM_NAME "_str2val\n", lookup_str);				\
	return NAMESPACE##_##ENUM_NAME##_LOOKUPERROR;													\
}

#define BEGIN_LONGENUM BEGIN_ENUM
#define LONGENUM_ELEMENT(element, index, str, longstr) ENUM_ELEMENT(element, index, str)
#define END_LONGENUM(NAMESPACE, ENUM_NAME) END_ENUM(NAMESPACE, ENUM_NAME)										\
const char * NAMESPACE##_##ENUM_NAME##2longstr(NAMESPACE##_##ENUM_NAME##_t value) {									\
	if (value >= NAMESPACE##_##ENUM_NAME##_exists[0] && value <= NAMESPACE##_##ENUM_NAME##_exists[ARRAY_LEN(NAMESPACE##_##ENUM_NAME##_exists)-1]) {	\
		return NAMESPACE##_##ENUM_NAME##_map[value].longstr;											\
	} else {																	\
		pbx_log(LOG_NOTICE, "SCCP: Error during lookup of '%d' in " #NAMESPACE "_" #ENUM_NAME "_val2longstr\n", value);				\
		return "SCCP: OutOfBounds Error during lookup of " #NAMESPACE "_" #ENUM_NAME "\n";											\
	}																		\
}																			\
NAMESPACE##_##ENUM_NAME##_t NAMESPACE##_##ENUM_NAME##_longstr2val(const char * lookup_longstr) {							\
	int idx;																	\
	for(idx=0; idx<=ARRAY_LEN(NAMESPACE##_##ENUM_NAME##_exists) - 1; idx++) {										\
		if (!strcasecmp(NAMESPACE##_##ENUM_NAME##_map[NAMESPACE##_##ENUM_NAME##_exists[idx]].longstr, lookup_longstr)) {			\
			return NAMESPACE##_##ENUM_NAME##_exists[idx];											\
		}																	\
	}																		\
	pbx_log(LOG_NOTICE, "SCCP: Error during lookup of '%s' in " #NAMESPACE "_" #ENUM_NAME "_longstr2val\n", lookup_longstr);			\
	return NAMESPACE##_##ENUM_NAME##_LOOKUPERROR;													\
}
#include ENUMMACRO_FILE
#endif

// exit
#undef BEGIN_ENUM
#undef ENUM_ELEMENT
#undef END_ENUM
#undef BEGIN_LONGENUM
#undef LONGENUM_ELEMENT
#undef END_LONGENUM

#undef ENUMMACRO_GENERATE
#undef ENUMMACRO_FILE
#undef ENUMMACRO_STAGE

