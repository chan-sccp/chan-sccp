/*!
 * \file 	sccp_config.c
 * \brief 	SCCP Config Class
 * \author 	Sergio Chersovani <mlists [at] c-net.it>
 * \note	Reworked, but based on chan_sccp code.
 *        	The original chan_sccp driver that was made by Zozo which itself was derived from the chan_skinny driver.
 *        	Modified by Jan Czmok and Julien Goodwin
 * \note        This program is free software and may be modified and distributed under the terms of the GNU Public License.
 *		See the LICENSE file at the top of the source tree.
 * \note 	To find out more about the reload function see \ref sccp_config_reload
 * \remarks     Only methods directly related to chan-sccp configuration should be stored in this source file.
 *
 * $Date: 2010-11-17 18:10:34 +0100 (Wed, 17 Nov 2010) $
 * $Revision: 2154 $
 */

//#include <asterisk/paths.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include "../../src/config.h"
#include "gen_sccpconf.h"

#define CONFIG_TYPE_ALL 0
#define CONFIG_TYPE_DEFAULTS 1
#define CONFIG_TYPE_TEMPLATED 2
#define CONFIG_TYPE_SHORT 3
#define CONFIG_TYPE_XML 4
#define CONFIG_TYPE_MYSQL 4
//#define CONFIG_TYPE_JSON 5
char *replace(const char *s, const char *old, const char *new);

static int sccp_config_generate(const char *filename, size_t sizeof_filename, int config_type)  {
        const SCCPConfigSegment *sccpConfigSegment = NULL;
        const SCCPConfigOption *config=NULL;
        long unsigned int sccp_option;
        long unsigned int segment;
        char *description;
        char *description_part;
        char name_and_value[100];
        int linelen=0;

//	snprintf(fn, sizeof(fn), "%s/%s", ast_config_AST_CONFIG_DIR, "sccp.conf.test");
        printf("info:" "Creating new config file '%s'\n", filename);
	
        FILE *f;
        if (!(f = fopen(filename, "w"))) {
                printf("error:" "Error creating new config file \n");
                return 1;
        }

        char date[256]="";
        time_t t;
        time(&t);
        strncpy(date, ctime(&t), sizeof(date));
                                
        if (CONFIG_TYPE_MYSQL == config_type) {
                int first_column_printed=0;
                fprintf(f,"/*\n");
                fprintf(f," * Automatically generated configuration file\n");
                fprintf(f," * Filename: %s\n", filename);
                fprintf(f," * Generator: gen_sccpconf\n");
                fprintf(f," * Creation Date: %s", date);
                fprintf(f," * Version: %s\n", SCCP_VERSION);
                fprintf(f," * Revision: %s\n", SCCP_REVISION);
                fprintf(f," */\n");
                for (segment=SCCP_CONFIG_GLOBAL_SEGMENT; segment <= SCCP_CONFIG_SOFTKEY_SEGMENT; segment++) {			
                        sccpConfigSegment = sccp_find_segment(segment);
                        printf("info:" "adding [%s] section\n", sccpConfigSegment->name);
                        
                        fprintf(f, "\n");
                        fprintf(f, "--\n");
                        fprintf(f, "-- %s\n", sccpConfigSegment->name);
                        fprintf(f, "--\n");
                        fprintf(f, "DROP TABLE IF EXISTS `sccp%s`;\n", sccpConfigSegment->name);	// optional
                        fprintf(f, "CREATE TABLE IF NOT EXISTS `sccp%s` (\n", sccpConfigSegment->name);
                        config = sccpConfigSegment->config;
                        for (sccp_option = 0; sccp_option < sccpConfigSegment->config_size; sccp_option++) {
                                if ((config[sccp_option].flags & SCCP_CONFIG_FLAG_IGNORE & SCCP_CONFIG_FLAG_DEPRECATED & SCCP_CONFIG_FLAG_OBSOLETE) == 0) {
                                        printf("info:" "adding name: %s, default_value: %s\n", config[sccp_option].name, config[sccp_option].defaultValue);

                                        if (!first_column_printed) {
                                                fprintf(f, "  `%s`", config[sccp_option].name);
                                                first_column_printed = 1;
                                        } else { 
                                                fprintf(f, ",\n  `%s`", config[sccp_option].name);
                                        }        
                                        if (config[sccp_option].type) {
                                                switch (config[sccp_option].type) {
                                                        case SCCP_CONFIG_DATATYPE_BOOLEAN:
                                                                fprintf(f, " ENUM('yes','no')");
                                                                if (config[sccp_option].defaultValue && !strlen(config[sccp_option].defaultValue)==0) {
                                                                        fprintf(f, " DEFAULT '%s'", config[sccp_option].defaultValue);
                                                                }
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_INT:
                                                                fprintf(f, " INT");
                                                                if (config[sccp_option].defaultValue && !strlen(config[sccp_option].defaultValue)==0) {
                                                                        fprintf(f, " DEFAULT %d", atoi(config[sccp_option].defaultValue));
                                                                }
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_UINT:
                                                                fprintf(f, " INT UNSIGNED");
                                                                if (config[sccp_option].defaultValue && !strlen(config[sccp_option].defaultValue)==0) {
                                                                        fprintf(f, " DEFAULT %d", atoi(config[sccp_option].defaultValue));
                                                                }
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_STRINGPTR:
                                                        case SCCP_CONFIG_DATATYPE_STRING:
                                                                if (config[sccp_option].defaultValue && !strlen(config[sccp_option].defaultValue)==0) {
                                                                        fprintf(f, " VARCHAR(%d)", strlen(config[sccp_option].defaultValue) > 45 ? (int) strlen(config[sccp_option].defaultValue)*2 : 45);
                                                                        fprintf(f, " DEFAULT '%s'", config[sccp_option].defaultValue);
                                                                } else {
                                                                        fprintf(f, " VARCHAR(%d)", 45);
                                                                }                                                                
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_PARSER:
                                                                fprintf(f, " VARCHAR(45)");
/*                                                                if (((config[sccp_option].flags & SCCP_CONFIG_FLAG_MULTI_ENTRY) == SCCP_CONFIG_FLAG_MULTI_ENTRY)) {
                                                                        fprintf(f, " SET(%s)\n", config[sccp_option].generic_parser);
                                                                } else {
                                                                        fprintf(f, " ENUM(%s)", config[sccp_option].generic_parser);
                                                                }
*/                                                                
//                                                                fprintf(f, "        <generic_parser>%s</generic_parser>\n", config[sccp_option].generic_parser);
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_CHAR:
                                                                fprintf(f, " CHAR(1)");
                                                                if (config[sccp_option].defaultValue && !strlen(config[sccp_option].defaultValue)==0) {
                                                                        fprintf(f, " DEFAULT '%-1s'", config[sccp_option].defaultValue);
                                                                }
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_ENUM2INT:
                                                        case SCCP_CONFIG_DATATYPE_ENUM2STR:
                                                        case SCCP_CONFIG_DATATYPE_CSV2STR:
                                                                break;
                                                }
                                        }
                                        fprintf(f, " %s", ((config[sccp_option].flags & SCCP_CONFIG_FLAG_REQUIRED) == SCCP_CONFIG_FLAG_REQUIRED) ? "NOT NULL" : "");
                                        if (strlen(config[sccp_option].description)!=0) {
                                                fprintf(f, " COMMENT '");
                                                description=malloc(sizeof(char) * strlen(config[sccp_option].description));	
                                                description=strdup(config[sccp_option].description);	
                                                while ((description_part=strsep(&description, "\n"))) {
                                                        if (description_part && strlen(description_part)!=0) {
                                                                fprintf(f, " %s ", replace(description_part,"'","\\'"));
                                                        }
                                                }
                                                fprintf(f, "'");
                                        }
                                }
                        }
//                        fprintf(f, " PRIMARY KEY ('')\n");
                        fprintf(f, "\n);\n");
                        first_column_printed = 0;
                }
                fprintf(f, "\n");
        } else if (CONFIG_TYPE_XML == config_type) {
                fprintf(f, "<?xml version=\"1.0\"?>\n");
                fprintf(f, "<sccp>\n");
                fprintf(f, "  <version>%s</version>\n", SCCP_VERSION);
                fprintf(f, "  <revision>%s</revision>\n", SCCP_REVISION);
                for (segment=SCCP_CONFIG_GLOBAL_SEGMENT; segment <= SCCP_CONFIG_SOFTKEY_SEGMENT; segment++) {			
                        sccpConfigSegment = sccp_find_segment(segment);
                        printf("info:" "adding [%s] section\n", sccpConfigSegment->name);
                        
                        fprintf(f, "  <section name=\"%s\">\n", sccpConfigSegment->name);
                        fprintf(f, "    <params>\n");
                        config = sccpConfigSegment->config;
                        for (sccp_option = 0; sccp_option < sccpConfigSegment->config_size; sccp_option++) {
                                if ((config[sccp_option].flags & SCCP_CONFIG_FLAG_IGNORE & SCCP_CONFIG_FLAG_DEPRECATED & SCCP_CONFIG_FLAG_OBSOLETE) == 0) {
                                        printf("info:" "adding name: %s, default_value: %s\n", config[sccp_option].name, config[sccp_option].defaultValue);

                                        fprintf(f, "      <param name=\"%s\">\n", config[sccp_option].name);
                                        fprintf(f, "        <required>%d</required>\n", ((config[sccp_option].flags & SCCP_CONFIG_FLAG_REQUIRED) == SCCP_CONFIG_FLAG_REQUIRED) ? 1 : 0);
                                        if (((config[sccp_option].flags & SCCP_CONFIG_FLAG_MULTI_ENTRY) == SCCP_CONFIG_FLAG_MULTI_ENTRY)) {
                                                fprintf(f, "        <multiple>1</multiple>\n");
                                        }
                                        if (config[sccp_option].type) {
                                                switch (config[sccp_option].type) {
                                                        case SCCP_CONFIG_DATATYPE_BOOLEAN:
                                                                fprintf(f, "        <type>boolean</type>\n");
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_INT:
                                                                fprintf(f, "        <type>int</type>\n");
                                                                fprintf(f, "        <size>8</size>\n");
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_UINT:
                                                                fprintf(f, "        <type>unsigned int</type>\n");
                                                                fprintf(f, "        <size>8</size>\n");
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_STRING:
                                                                fprintf(f, "        <type>string</type>\n");
                                                                fprintf(f, "        <size>45</size>\n");
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_PARSER:
                                                                fprintf(f, "        <type>generic</type>\n");
                                                                fprintf(f, "        <generic_parser>%s</generic_parser>\n", config[sccp_option].generic_parser);
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_STRINGPTR:
                                                                fprintf(f, "        <type>string</type>\n");
                                                                fprintf(f, "        <size>45</size>\n");
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_CHAR:
                                                                fprintf(f, "        <type>char</type>\n");
                                                                break;
                                                        case SCCP_CONFIG_DATATYPE_ENUM2INT:
                                                        case SCCP_CONFIG_DATATYPE_ENUM2STR:
                                                        case SCCP_CONFIG_DATATYPE_CSV2STR:
                                                                break;
                                                }
                                        }
                                        if (config[sccp_option].defaultValue && !strlen(config[sccp_option].defaultValue)==0) {
                                                fprintf(f, "        <default>%s</default>\n", config[sccp_option].defaultValue);
                                        }
                                        if (strlen(config[sccp_option].description)!=0) {
                                                fprintf(f, "        <description>");
                                                description=malloc(sizeof(char) * strlen(config[sccp_option].description));	
                                                description=strdup(config[sccp_option].description);	
                                                while ((description_part=strsep(&description, "\n"))) {
                                                        if (description_part && strlen(description_part)!=0) {
                                                                fprintf(f, "%s ", replace(description_part,"'","\\'"));
                                                        }
                                                }
                                                fprintf(f, "</description>\n");
                                        }
                                        fprintf(f, "      </param>\n");
                                }
                        }
                        fprintf(f, "    </params>\n");
                        fprintf(f, "  </section>\n");
                }
                fprintf(f, "</sccp>\n");
        } else {
                fprintf(f,";!\n");
                fprintf(f,";! Automatically generated configuration file\n");
                fprintf(f,";! Filename: %s\n", filename);
                fprintf(f,";! Generator: sccp config generate\n");
                fprintf(f,";! Creation Date: %s", date);
                fprintf(f,";!\n");
                fprintf(f,"\n");

                for (segment=SCCP_CONFIG_GLOBAL_SEGMENT; segment <= SCCP_CONFIG_SOFTKEY_SEGMENT; segment++) {			
                        sccpConfigSegment = sccp_find_segment(segment);
                        printf("info:" "adding [%s] section\n", sccpConfigSegment->name);
                        if (CONFIG_TYPE_TEMPLATED == config_type && SCCP_CONFIG_DEVICE_SEGMENT == segment) {
                                fprintf(f, "\n;\n; %s section\n;\n[%s_template](!)                                                              ; create new template\n", sccpConfigSegment->name, sccpConfigSegment->name);
                        } else if (CONFIG_TYPE_TEMPLATED == config_type && SCCP_CONFIG_LINE_SEGMENT == segment) {
                                fprintf(f, "\n;\n; %s section\n;\n[%s_template](!)                                                                ; create new template\n", sccpConfigSegment->name, sccpConfigSegment->name);
                        } else {
                                fprintf(f, "\n;\n; %s section\n;\n[%s]\n", sccpConfigSegment->name, sccpConfigSegment->name);
                        }

                        config = sccpConfigSegment->config;
                        for (sccp_option = 0; sccp_option < sccpConfigSegment->config_size; sccp_option++) {
                                if ((config[sccp_option].flags & SCCP_CONFIG_FLAG_IGNORE & SCCP_CONFIG_FLAG_DEPRECATED & SCCP_CONFIG_FLAG_OBSOLETE) == 0) {
                                        printf("info:" "adding name: %s, default_value: %s\n", config[sccp_option].name, config[sccp_option].defaultValue);
                                        
                                        if (config[sccp_option].name && strlen(config[sccp_option].name)!=0) {
                                                switch (config_type){
                                                        case CONFIG_TYPE_ALL:
                                                                break;	
                                                        case CONFIG_TYPE_DEFAULTS:
                                                                if ( (((config[sccp_option].flags & SCCP_CONFIG_FLAG_REQUIRED) != SCCP_CONFIG_FLAG_REQUIRED) && (!config[sccp_option].defaultValue || strlen(config[sccp_option].defaultValue)==0)) ) {
                                                                        continue;
                                                                }
                                                                break;	
                                                        case CONFIG_TYPE_TEMPLATED:
                                                                if ( (!config[sccp_option].defaultValue || strlen(config[sccp_option].defaultValue)==0) ) {
                                                                        continue;
                                                                }
                                                                break;	
                                                        case CONFIG_TYPE_SHORT:		// SHORT
                                                                if ( (((config[sccp_option].flags & SCCP_CONFIG_FLAG_REQUIRED) != SCCP_CONFIG_FLAG_REQUIRED) && (!config[sccp_option].defaultValue || strlen(config[sccp_option].defaultValue)==0)) ) {
                                                                        continue;
                                                                }
                                                                break;	
                                                }
                                                if (config[sccp_option].defaultValue && !strlen(config[sccp_option].defaultValue)==0) {
                                                        snprintf(name_and_value, sizeof(name_and_value),"%s = %s", config[sccp_option].name, config[sccp_option].defaultValue);
                                                } else {
                                                        snprintf(name_and_value, sizeof(name_and_value),"%s = \"\"", config[sccp_option].name);
                                                }							
                                                linelen=(int)strlen(name_and_value);
                                                fprintf(f, "%s", name_and_value);
                                                if (CONFIG_TYPE_SHORT != config_type && config[sccp_option].description && strlen(config[sccp_option].description)!=0) {
                                                        description=malloc(sizeof(char) * strlen(config[sccp_option].description));	
                                                        description=strdup(config[sccp_option].description);	
                                                        while ((description_part=strsep(&description, "\n"))) {
                                                                if (description_part && strlen(description_part)!=0) {
                                                                        fprintf(f, "%*.s ; %s%s%s\n", 81-linelen, " ", (config[sccp_option].flags & SCCP_CONFIG_FLAG_REQUIRED) == SCCP_CONFIG_FLAG_REQUIRED ? "(REQUIRED) " : "", ((config[sccp_option].flags & SCCP_CONFIG_FLAG_MULTI_ENTRY) == SCCP_CONFIG_FLAG_MULTI_ENTRY) ? "(MULTI-ENTRY) " : "", description_part);
                                                                        linelen=0;
                                                                }
                                                        }	
                                                } else {
                                                        fprintf(f, "\n");
                                                }
                                        } else {
                                                printf("error:" "Error creating new variable structure for %s='%s'\n", config[sccp_option].name, config[sccp_option].defaultValue);
                                                return 2;
                                        }
                                }
                        }
                        printf("\n");
                        if (CONFIG_TYPE_TEMPLATED == config_type) {
                                // make up two devices and two lines
                                if (SCCP_CONFIG_DEVICE_SEGMENT == segment) {
                                        fprintf(f, "\n");
                                        fprintf(f, "[SEP001B535CD5E4](device_template)                                                ; use template 'device_template'\n");
                                        fprintf(f, "description=sample1\n");
                                        fprintf(f, "devicetype=7960\n");
                                        fprintf(f, "button=line, 110\n");
                                        fprintf(f, "button=line, 200@01:_SharedLine\n");
                                        fprintf(f, "\n");
                                        fprintf(f, "[SEP0024C4444763](device_template)                                                ; use template 'device_template'\n");
                                        fprintf(f, "description=sample1\n");
                                        fprintf(f, "devicetype=7962\n");
                                        fprintf(f, "button=line, 111\n");
                                        fprintf(f, "button=line, 200@02:_SharedLine\n");
                                        fprintf(f, "\n");
                                }
                                if (SCCP_CONFIG_LINE_SEGMENT == segment) {
                                        fprintf(f, "\n");
                                        fprintf(f, "[110](line_template)                                                              ; use template 'line_template'\n");
                                        fprintf(f, "id=110\n");
                                        fprintf(f, "label=sample_110\n");
                                        fprintf(f, "description=sampleline_110\n");
                                        fprintf(f, "cid_num=110\n");
                                        fprintf(f, "cid_name=user110\n");
                                        fprintf(f, "\n");
                                        fprintf(f, "[111](line_template)                                                              ; use template 'line_template'\n");
                                        fprintf(f, "id=111\n");
                                        fprintf(f, "label=sample_111\n");
                                        fprintf(f, "description=sampleline_111\n");
                                        fprintf(f, "cid_num=111\n");
                                        fprintf(f, "cid_name=user111\n");
                                        fprintf(f, "\n");
                                        fprintf(f, "[200](line_template)                                                              ; use template 'line_template'\n");
                                        fprintf(f, "id=200\n");
                                        fprintf(f, "label=sample_share\n");
                                        fprintf(f, "description=sharedline\n");
                                        fprintf(f, "cid_num=200\n");
                                        fprintf(f, "cid_name=shared\n");
                                        fprintf(f, "\n");
                                }
                        
                        }
                }
	}
        fclose(f);

	return 0;
}

char *replace(const char *s, const char *old, const char *new)
{
        char *ret;
        int i, count = 0;
        size_t newlen = strlen(new);
        size_t oldlen = strlen(old);

        for (i = 0; s[i] != '\0'; i++) {
                if (strstr(&s[i], old) == &s[i]) {
                        count++;
                        i += oldlen - 1;
                }
        }
        ret = malloc(i + count * (newlen - oldlen));
        if (ret == NULL)
                exit(EXIT_FAILURE);

        i = 0;
        while (*s) {
                if (strstr(s, old) == s) {
                        strcpy(&ret[i], new);
                        i += newlen;
                        s += oldlen;
                } else
                        ret[i++] = *s++;
        }
        ret[i] = '\0';

        return ret;
}        

int main(int argc, char *argv[])
{
	char *config_file="";
	int  config_type=0;
	
	if(argc>2) {
		config_file=malloc(sizeof(char*) * strlen(argv[1]) + 1);
		config_file=strdup(argv[1]);	

		if (!strcasecmp(argv[2], "ALL")) {
			config_type=CONFIG_TYPE_ALL;
		}
		else if (!strcasecmp(argv[2], "DEFAULTS")) {
			config_type=CONFIG_TYPE_DEFAULTS;
		}
		else if (!strcasecmp(argv[2], "TEMPLATED")) {
			config_type=CONFIG_TYPE_TEMPLATED;
		}
		else if (!strcasecmp(argv[2], "SHORT")) {
			config_type=CONFIG_TYPE_SHORT;	
		}	
		else if (!strcasecmp(argv[2], "XML")) {
			config_type=CONFIG_TYPE_XML;	
		}
		else if (!strcasecmp(argv[2], "MYSQL")) {
			config_type=CONFIG_TYPE_MYSQL;	
		}
	} else {
		printf("Usae: gen_sccpconf <config filename> <conf_type>\n");	
		printf("      where conf_type is one of: (ALL | DEFAULTS | TEMPLATED | SHORT | XML)\n");
		return 1;
	}
	
	return sccp_config_generate(config_file, sizeof(config_file), config_type);
}

