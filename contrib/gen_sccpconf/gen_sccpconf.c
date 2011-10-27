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
							if ( (((config[sccp_option].flags & SCCP_CONFIG_FLAG_REQUIRED) == 0) && (!config[sccp_option].defaultValue || strlen(config[sccp_option].defaultValue)==0)) ) {
								continue;
							}
							break;	
						case CONFIG_TYPE_TEMPLATED:
							if ( (!config[sccp_option].defaultValue || strlen(config[sccp_option].defaultValue)==0) ) {
								continue;
							}
							break;	
						case CONFIG_TYPE_SHORT:		// SHORT
							if ( (((config[sccp_option].flags & SCCP_CONFIG_FLAG_REQUIRED) == 0) && (!config[sccp_option].defaultValue || strlen(config[sccp_option].defaultValue)==0)) ) {
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
								fprintf(f, "%*.s ; %s%s\n", 81-linelen, " ", (config[sccp_option].flags & SCCP_CONFIG_FLAG_REQUIRED) != 0 ? "(REQUIRED) " : "", description_part);
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
        fclose(f);

	return 0;
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
	} else {
		printf("Usae: gen_sccpconf <config filename> <conf_type>\n");	
		printf("      where conf_type is one of: ALL | DEFAULTS | TEMPLATED | SHORT\n");
		return 1;
	}
	
	return sccp_config_generate(config_file, sizeof(config_file), config_type);
}

