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

static int sccp_config_generate(char *filename, size_t sizeof_filename)  {
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
		fprintf(f, "\n;\n; %s section\n;\n[%s]\n", sccpConfigSegment->name, sccpConfigSegment->name);

		config = sccpConfigSegment->config;
		for (sccp_option = 0; sccp_option < sccpConfigSegment->config_size; sccp_option++) {
			if ((config[sccp_option].flags & SCCP_CONFIG_FLAG_IGNORE & SCCP_CONFIG_FLAG_DEPRECATED & SCCP_CONFIG_FLAG_OBSOLETE) == 0) {
				printf("info:" "adding name: %s, default_value: %s\n", config[sccp_option].name, config[sccp_option].defaultValue);
				
				if (config[sccp_option].name && strlen(config[sccp_option].name)!=0) {
					if ((config[sccp_option].defaultValue && strlen(config[sccp_option].defaultValue)!=0)		// non empty
						|| ((config[sccp_option].flags & SCCP_CONFIG_FLAG_REQUIRED) != 0 && (config[sccp_option].defaultValue && strlen(config[sccp_option].defaultValue)==0)) // empty but required
					) {
						snprintf(name_and_value, sizeof(name_and_value),"%s = %s", config[sccp_option].name, strlen(config[sccp_option].defaultValue)==0 ? "\"\"" : config[sccp_option].defaultValue);
						linelen=(int)strlen(name_and_value);
						fprintf(f, "%s", name_and_value);
						if (config[sccp_option].description && strlen(config[sccp_option].description)!=0) {
							description=malloc(sizeof(char) * strlen(config[sccp_option].description));	
							description=strdup(config[sccp_option].description);	
							while ((description_part=strsep(&description, "\n"))) {
								if (description_part && strlen(description_part)!=0) {
									fprintf(f, "%*.s ; %s\n", 81-linelen, " ", description_part);
									linelen=0;
								}
							}	
						} else {
							fprintf(f, "\n");
						}
					}
				} else {
					printf("error:" "Error creating new variable structure for %s='%s'\n", config[sccp_option].name, config[sccp_option].defaultValue);
					return 2;
				}
			}
		}
		printf("\n");
	}
        fclose(f);

	return 0;
}

int main(int argc, char *argv[])
{
	char *config_file="";
	
	if(argc>1) {
		config_file=malloc(sizeof(char*) * strlen(argv[1]) + 1);
		config_file=strdup(argv[1]);	
	} else {
		printf("Usage: gen_sccpconf <config filename>\n");	
		return 1;
	}
	
	if (sccp_config_generate(config_file, sizeof(config_file))) {
		return 0;
	} else {
		return 255;
	}
}

