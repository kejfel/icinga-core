 /*****************************************************************************
 *
 * XODTEMPLATE.C - Template-based object configuration data input routines
 *
 * Copyright (c) 2001-2002 Ethan Galstad (nagios@nagios.org)
 * Last Modified: 04-30-2002
 *
 * Description:
 *
 * Routines for parsing and resolving template-based object definitions.
 * Basic steps involved in this are as follows:
 *
 *    1) Read
 *    2) Resolve
 *    3) Replicate
 *    4) Register
 *    5) Cleanup
 *
 *
 * License:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *****************************************************************************/


/*********** COMMON HEADER FILES ***********/

#include "../common/config.h"
#include "../common/common.h"
#include "../common/objects.h"

/**** CORE OR CGI SPECIFIC HEADER FILES ****/

#ifdef NSCORE
#include "../base/nagios.h"
#endif

#ifdef NSCGI
#include "../cgi/cgiutils.h"
#endif


/**** DATA INPUT-SPECIFIC HEADER FILES ****/

#include "xodtemplate.h"



xodtemplate_timeperiod *xodtemplate_timeperiod_list=NULL;
xodtemplate_command *xodtemplate_command_list=NULL;
xodtemplate_contactgroup *xodtemplate_contactgroup_list=NULL;
xodtemplate_hostgroup *xodtemplate_hostgroup_list=NULL;
xodtemplate_servicedependency *xodtemplate_servicedependency_list=NULL;
xodtemplate_serviceescalation *xodtemplate_serviceescalation_list=NULL;
xodtemplate_hostgroupescalation *xodtemplate_hostgroupescalation_list=NULL;
xodtemplate_contact *xodtemplate_contact_list=NULL;
xodtemplate_host *xodtemplate_host_list=NULL;
xodtemplate_service *xodtemplate_service_list=NULL;
xodtemplate_hostdependency *xodtemplate_hostdependency_list=NULL;
xodtemplate_hostescalation *xodtemplate_hostescalation_list=NULL;

void *xodtemplate_current_object=NULL;
int xodtemplate_current_object_type=XODTEMPLATE_NONE;






/******************************************************************/
/************* TOP-LEVEL CONFIG DATA INPUT FUNCTION ***************/
/******************************************************************/

/* process all config files - both core and CGIs pass in name of main config file */
int xodtemplate_read_config_data(char *main_config_file,int options){
	char config_file[MAX_FILENAME_LENGTH];
	char input[MAX_XODTEMPLATE_INPUT_BUFFER];
	char *temp_ptr;
	FILE *fp;
	int result=OK;

#ifdef DEBUG0
	printf("xodtemplate_read_config_data() start\n");
#endif

	/* open the main config file for reading (we need to find all the config files to read) */
	fp=fopen(main_config_file,"r");
	if(fp==NULL){
#ifdef DEBUG1
		printf("Error: Cannot open main configuration file '%s' for reading!\n",main_config_file);
#endif
		return ERROR;
	        }

	/* initialize object definition lists to NULL - CGIs core dump w/o this (why?) */
	xodtemplate_timeperiod_list=NULL;
	xodtemplate_command_list=NULL;
	xodtemplate_contactgroup_list=NULL;
	xodtemplate_hostgroup_list=NULL;
	xodtemplate_servicedependency_list=NULL;
	xodtemplate_serviceescalation_list=NULL;
	xodtemplate_hostgroupescalation_list=NULL;
	xodtemplate_contact_list=NULL;
	xodtemplate_host_list=NULL;
	xodtemplate_service_list=NULL;
	xodtemplate_hostdependency_list=NULL;
	xodtemplate_hostescalation_list=NULL;

	/* read in all lines from the main config file */
	for(fgets(input,sizeof(input)-1,fp);!feof(fp);fgets(input,sizeof(input)-1,fp)){

		/* skip blank lines and comments */
		if(input[0]=='#' || input[0]==';' || input[0]=='\x0' || input[0]=='\n' || input[0]=='\r')
			continue;

		/* strip input */
		xodtemplate_strip(input);

		temp_ptr=strtok(input,"=");
		if(temp_ptr==NULL)
			continue;

		/* process a single config file */
		if(strstr(temp_ptr,"xodtemplate_config_file")==temp_ptr || strstr(temp_ptr,"cfg_file")==temp_ptr){

			/* get the config file name */
			temp_ptr=strtok(NULL,"\n");
			if(temp_ptr==NULL)
				continue;

			strncpy(config_file,temp_ptr,sizeof(config_file)-1);
			config_file[sizeof(config_file)-1]='\x0';

			/* process the config file... */
			result=xodtemplate_process_config_file(config_file,options);

			/* if there was an error processing the config file, break out of loop */
			if(result==ERROR)
				break;
		        }

		/* process all files in a config directory */
		else if(strstr(temp_ptr,"xodtemplate_config_dir")==temp_ptr || strstr(temp_ptr,"cfg_dir")==temp_ptr){

			/* get the config directory name */
			temp_ptr=strtok(NULL,"\n");
			if(temp_ptr==NULL)
				continue;

			strncpy(config_file,temp_ptr,sizeof(config_file)-1);
			config_file[sizeof(config_file)-1]='\x0';

			/* strip trailing / if necessary */
			if(config_file[strlen(config_file)-1]=='/')
				config_file[strlen(config_file)-1]='\x0';

			/* process the config directory... */
			result=xodtemplate_process_config_dir(config_file,options);

			/* if there was an error processing the config file, break out of loop */
			if(result==ERROR)
				break;
		        }
	        }

	fclose(fp);

	/* resolve objects definitions */
	if(result==OK)
		result=xodtemplate_resolve_objects();

	/* duplicate object definitions */
	if(result==OK)
		result=xodtemplate_duplicate_objects();

	/* register objects */
	if(result==OK)
		result=xodtemplate_register_objects();

	/* cleanup */
	xodtemplate_free_memory();

#ifdef DEBUG0
	printf("xodtemplate_read_config_data() end\n");
#endif

	return result;
	}



/* process all files in a specific config directory */
int xodtemplate_process_config_dir(char *dirname, int options){
	char config_file[MAX_FILENAME_LENGTH];
	DIR *dirp;
	struct dirent *dirfile;
	int result=OK;
	int x;

#ifdef DEBUG0
	printf("xodtemplate_process_config_dir() start\n");
#endif

	/* open the directory for reading */
	dirp=opendir(dirname);
        if(dirp==NULL){
#ifdef NSCORE
		printf("Error: Could not open config directory '%s' for reading.\n",dirname);
#endif
		result=ERROR;
	        }

	/* process all files in the directory... */
	while((dirfile=readdir(dirp))!=NULL){

		/* test if this is a config file, otherwise skip it... */
		x=strlen(dirfile->d_name);
		if(x<=4)
			continue;
		if(strcmp(dirfile->d_name+(x-4),".cfg"))
			continue;

		/* create the full path to the config file */
		snprintf(config_file,sizeof(config_file)-1,"%s/%s",dirname,dirfile->d_name);
		config_file[sizeof(config_file)-1]='\x0';

		/* process the config file */
		result=xodtemplate_process_config_file(config_file,options);

		/* break out if we encountered an error */
		if(result==ERROR)
			break;
		}

	closedir(dirp);

#ifdef DEBUG0
	printf("xodtemplate_process_config_dir() end\n");
#endif

	return result;
        }


/* process data in a specific config file */
int xodtemplate_process_config_file(char *filename, int options){
	FILE *fp;
	char input[MAX_XODTEMPLATE_INPUT_BUFFER];
	register in_definition=FALSE;
	char *temp_ptr;
	register current_line=0;
	int result=OK;
	register int x;
	register int y;

#ifdef DEBUG0
	printf("xodtemplate_process_config_file() start\n");
#endif

	/* open the config file for reading */
	fp=fopen(filename,"r");
	if(fp==NULL){
#ifdef NSCORE
		printf("Error: Cannot open config file '%s' for reading!\n",filename);
#endif
		return ERROR;
	        }

	/* read in all lines from the config file */
	while(fgets(input,sizeof(input)-1,fp)){

		current_line++;

		/* skip empty lines */
		if(input[0]=='#' || input[0]==';' || input[0]=='\r' || input[0]=='\n')
			continue;

		/* grab data before comment delimiter - faster than a strtok() and strncpy()... */
		for(x=0;input[x]!='\x0';x++)
			if(input[x]==';')
				break;
		input[x]='\x0';

		/* strip input */
		xodtemplate_strip(input);

		/* skip blank lines */
		if(input[0]=='\x0')
			continue;

		/* this is the start of an object definition */
		else if(strstr(input,"define ")==input){

			/* get the type of object we're defining... */
			for(x=7;input[x]!='\x0';x++)
				if(input[x]!=' ' && input[x]!='\t')
					break;
			for(y=0;input[x]!='\x0';x++){
				if(input[x]==' ' || input[x]=='\t' ||  input[x]=='{')
					break;
				else
					input[y++]=input[x];
			        }
			input[y]='\x0';

			/* make sure an object type is specified... */
			if(input[0]=='\x0'){
#ifdef NSCORE
				printf("Error: No object type specified in file '%s' on line %d.\n",filename,current_line);
#endif
				result=ERROR;
				break;
			        }

			/* check validity of object type */
			if(strcmp(input,"timeperiod") && strcmp(input,"command") && strcmp(input,"contact") && strcmp(input,"contactgroup") && strcmp(input,"host") && strcmp(input,"hostgroup") && strcmp(input,"service") && strcmp(input,"servicedependency") && strcmp(input,"serviceescalation") && strcmp(input,"hostgroupescalation") && strcmp(input,"hostdependency") && strcmp(input,"hostescalation")){
#ifdef NSCORE
				printf("Error: Invalid object definition type '%s' in file '%s' on line %d.\n",input,filename,current_line);
#endif
				result=ERROR;
				break;
			        }

			/* we're already in an object definition... */
			if(in_definition==TRUE){
#ifdef NSCORE
				printf("Error: Unexpected start of object definition in file '%s' on line %d.  Make sure you close preceding objects before starting a new one.\n",filename,current_line);
#endif				
				result=ERROR;
				break;
			        }

			/* start a new definition */
			if(xodtemplate_begin_object_definition(input,options)==ERROR){
#ifdef NSCORE
				printf("Error: Could not add object definition in file '%s' on line %d.\n",filename,current_line);
#endif
				result=ERROR;
				break;
			        }

			in_definition=TRUE;
		        }

		/* this is the close of an object definition */
		else if(!strcmp(input,"}") && in_definition==TRUE){

			in_definition=FALSE;

			/* close out current definition */
			if(xodtemplate_end_object_definition(options)==ERROR){
#ifdef NSCORE
				printf("Error: Could not complete object definition in file '%s' on line %d.\n",filename,current_line);
#endif
				result=ERROR;
				break;
			        }
		        }

		/* this is a directive inside an object definition */
		else if(in_definition==TRUE){
			
			/* add directive to object definition */
			if(xodtemplate_add_object_property(input,options)==ERROR){
#ifdef NSCORE
				printf("Error: Could not add object property in file '%s' on line %d.\n",filename,current_line);
#endif
				result=ERROR;
				break;
			        }
		        }

		/* unexpected token or statement */
		else{
#ifdef NSCORE
			printf("Error: Unexpected token or statement in file '%s' on line %d.\n",filename,current_line);
#endif
			result=ERROR;
			break;
		        }
	        }

	/* close file */
	fclose(fp);

	/* whoops - EOF while we were in the middle of an object definition... */
	if(in_definition==TRUE && result==OK){
#ifdef NSCORE
		printf("Error: Unexpected EOF in file '%s' on line %d - check for a missing closing bracket.\n",filename,current_line);
#endif
		result=ERROR;
	        }

#ifdef DEBUG0
	printf("xodtemplate_process_config_file() end\n");
#endif

	return result;
        }



/* strip newline, carriage return, and tab characters from beginning and end of a string */
void xodtemplate_strip(char *buffer){
	register int x;
	register int y;
	char ch;
	register int a;
	register int b;

	if(buffer==NULL || buffer[0]=='\x0')
		return;

	/* strip end of string */
	x=(int)strlen(buffer)-1;
	for(;x>=0;x--){
		if(buffer[x]==' ' || buffer[x]=='\n' || buffer[x]=='\r' || buffer[x]=='\t' || buffer[x]==13)
			buffer[x]='\x0';
		else
			break;
	        }

	/* reverse string */
	x=(int)strlen(buffer);
	if(x==0)
		return;
	for(a=0,b=x-1;a<b;a++,b--){
		ch=buffer[a];
		buffer[a]=buffer[b];
		buffer[b]=ch;
	        }

	/* strip beginning of string (now end of string) */
	x--;
	for(;x>=0;x--){
		if(buffer[x]==' ' || buffer[x]=='\n' || buffer[x]=='\r' || buffer[x]=='\t' || buffer[x]==13)
			buffer[x]='\x0';
		else
			break;
	        }

	/* reverse string again */
	x=(int)strlen(buffer);
	if(x==0)
		return;
	for(a=0,b=x-1;a<b;a++,b--){
		ch=buffer[a];
		buffer[a]=buffer[b];
		buffer[b]=ch;
	        }

	return;
	}





/******************************************************************/
/***************** OBJECT DEFINITION FUNCTIONS ********************/
/******************************************************************/

/* starts a new object definition */
int xodtemplate_begin_object_definition(char *input, int options){
	int result=OK;
	xodtemplate_timeperiod *new_timeperiod;
	xodtemplate_command *new_command;
	xodtemplate_contactgroup *new_contactgroup;
	xodtemplate_hostgroup *new_hostgroup;
	xodtemplate_servicedependency *new_servicedependency;
	xodtemplate_serviceescalation *new_serviceescalation;
	xodtemplate_hostgroupescalation *new_hostgroupescalation;
	xodtemplate_contact *new_contact;
	xodtemplate_host *new_host;
	xodtemplate_service *new_service;
	xodtemplate_hostdependency *new_hostdependency;
	xodtemplate_hostescalation *new_hostescalation;
	int x;

#ifdef DEBUG0
	printf("xodtemplate_begin_object_definition() start\n");
#endif

	if(!strcmp(input,"service"))
		xodtemplate_current_object_type=XODTEMPLATE_SERVICE;
	else if(!strcmp(input,"host"))
		xodtemplate_current_object_type=XODTEMPLATE_HOST;
	else if(!strcmp(input,"command"))
		xodtemplate_current_object_type=XODTEMPLATE_COMMAND;
	else if(!strcmp(input,"contact"))
		xodtemplate_current_object_type=XODTEMPLATE_CONTACT;
	else if(!strcmp(input,"contactgroup"))
		xodtemplate_current_object_type=XODTEMPLATE_CONTACTGROUP;
	else if(!strcmp(input,"hostgroup"))
		xodtemplate_current_object_type=XODTEMPLATE_HOSTGROUP;
	else if(!strcmp(input,"timeperiod"))
		xodtemplate_current_object_type=XODTEMPLATE_TIMEPERIOD;
	else if(!strcmp(input,"servicedependency"))
		xodtemplate_current_object_type=XODTEMPLATE_SERVICEDEPENDENCY;
	else if(!strcmp(input,"serviceescalation"))
		xodtemplate_current_object_type=XODTEMPLATE_SERVICEESCALATION;
	else if(!strcmp(input,"hostgroupescalation"))
		xodtemplate_current_object_type=XODTEMPLATE_HOSTGROUPESCALATION;
	else if(!strcmp(input,"hostdependency"))
		xodtemplate_current_object_type=XODTEMPLATE_HOSTDEPENDENCY;
	else if(!strcmp(input,"hostescalation"))
		xodtemplate_current_object_type=XODTEMPLATE_HOSTESCALATION;
	else
		return ERROR;


	/* check to see if we should process this type of object */
	switch(xodtemplate_current_object_type){
	case XODTEMPLATE_TIMEPERIOD:
		if(!(options & READ_TIMEPERIODS))
			return OK;
		break;
	case XODTEMPLATE_COMMAND:
		if(!(options & READ_COMMANDS))
			return OK;
		break;
	case XODTEMPLATE_CONTACT:
		if(!(options & READ_CONTACTS))
			return OK;
		break;
	case XODTEMPLATE_CONTACTGROUP:
		if(!(options & READ_CONTACTGROUPS))
			return OK;
		break;
	case XODTEMPLATE_HOST:
		if(!(options & READ_HOSTS))
			return OK;
		break;
	case XODTEMPLATE_HOSTGROUP:
		if(!(options & READ_HOSTGROUPS))
			return OK;
		break;
	case XODTEMPLATE_SERVICE:
		if(!(options & READ_SERVICES))
			return OK;
		break;
	case XODTEMPLATE_SERVICEDEPENDENCY:
		if(!(options & READ_SERVICEDEPENDENCIES))
			return OK;
		break;
	case XODTEMPLATE_SERVICEESCALATION:
		if(!(options & READ_SERVICEESCALATIONS))
			return OK;
		break;
	case XODTEMPLATE_HOSTGROUPESCALATION:
		if(!(options & READ_HOSTGROUPESCALATIONS))
			return OK;
		break;
	case XODTEMPLATE_HOSTDEPENDENCY:
		if(!(options & READ_HOSTDEPENDENCIES))
			return OK;
		break;
	case XODTEMPLATE_HOSTESCALATION:
		if(!(options & READ_HOSTESCALATIONS))
			return OK;
		break;
	default:
		return ERROR;
		break;
	        }



	/* add a new (blank) object */
	switch(xodtemplate_current_object_type){

	case XODTEMPLATE_TIMEPERIOD:

		/* allocate memory */
		new_timeperiod=(xodtemplate_timeperiod *)malloc(sizeof(xodtemplate_timeperiod));
		if(new_timeperiod==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for timeperiod definition\n");
#endif
			return ERROR;
		        }

		new_timeperiod->template=NULL;
		new_timeperiod->name=NULL;
		new_timeperiod->timeperiod_name=NULL;
		new_timeperiod->alias=NULL;
		for(x=0;x<7;x++)
			new_timeperiod->timeranges[x]=NULL;
		new_timeperiod->has_been_resolved=FALSE;
		new_timeperiod->register_object=TRUE;

		/* add new timeperiod to head of list in memory */
		new_timeperiod->next=xodtemplate_timeperiod_list;
		xodtemplate_timeperiod_list=new_timeperiod;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_timeperiod_list;

		break;

	case XODTEMPLATE_COMMAND:

		/* allocate memory */
		new_command=(xodtemplate_command *)malloc(sizeof(xodtemplate_command));
		if(new_command==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for command definition\n");
#endif
			return ERROR;
		        }
		
		new_command->template=NULL;
		new_command->name=NULL;
		new_command->command_name=NULL;
		new_command->command_line=NULL;
		new_command->has_been_resolved=FALSE;
		new_command->register_object=TRUE;

		/* add new command to head of list in memory */
		new_command->next=xodtemplate_command_list;
		xodtemplate_command_list=new_command;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_command_list;

		break;

	case XODTEMPLATE_CONTACTGROUP:

		/* allocate memory */
		new_contactgroup=(xodtemplate_contactgroup *)malloc(sizeof(xodtemplate_contactgroup));
		if(new_contactgroup==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for contactgroup definition\n");
#endif
			return ERROR;
		        }
		
		new_contactgroup->template=NULL;
		new_contactgroup->name=NULL;
		new_contactgroup->contactgroup_name=NULL;
		new_contactgroup->alias=NULL;
		new_contactgroup->members=NULL;
		new_contactgroup->has_been_resolved=FALSE;
		new_contactgroup->register_object=TRUE;

		/* add new contactgroup to head of list in memory */
		new_contactgroup->next=xodtemplate_contactgroup_list;
		xodtemplate_contactgroup_list=new_contactgroup;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_contactgroup_list;
		break;


	case XODTEMPLATE_HOSTGROUP:

		/* allocate memory */
		new_hostgroup=(xodtemplate_hostgroup *)malloc(sizeof(xodtemplate_hostgroup));
		if(new_hostgroup==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for hostgroup definition\n");
#endif
			return ERROR;
		        }
		
		new_hostgroup->template=NULL;
		new_hostgroup->name=NULL;
		new_hostgroup->hostgroup_name=NULL;
		new_hostgroup->alias=NULL;
		new_hostgroup->members=NULL;
		new_hostgroup->contact_groups=NULL;
		new_hostgroup->has_been_resolved=FALSE;
		new_hostgroup->register_object=TRUE;

		/* add new hostgroup to head of list in memory */
		new_hostgroup->next=xodtemplate_hostgroup_list;
		xodtemplate_hostgroup_list=new_hostgroup;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_hostgroup_list;
		break;

	case XODTEMPLATE_SERVICEDEPENDENCY:

		/* allocate memory */
		new_servicedependency=(xodtemplate_servicedependency *)malloc(sizeof(xodtemplate_servicedependency));
		if(new_servicedependency==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for servicedependency definition\n");
#endif
			return ERROR;
		        }
		
		new_servicedependency->template=NULL;
		new_servicedependency->name=NULL;
		new_servicedependency->host_name=NULL;
		new_servicedependency->service_description=NULL;
		new_servicedependency->dependent_host_name=NULL;
		new_servicedependency->dependent_service_description=NULL;
		new_servicedependency->fail_execute_on_ok=FALSE;
		new_servicedependency->fail_execute_on_unknown=FALSE;
		new_servicedependency->fail_execute_on_warning=FALSE;
		new_servicedependency->fail_execute_on_critical=FALSE;
		new_servicedependency->fail_notify_on_ok=FALSE;
		new_servicedependency->fail_notify_on_unknown=FALSE;
		new_servicedependency->fail_notify_on_warning=FALSE;
		new_servicedependency->fail_notify_on_critical=FALSE;
		new_servicedependency->have_execution_dependency_options=FALSE;
		new_servicedependency->have_notification_dependency_options=FALSE;
		new_servicedependency->has_been_resolved=FALSE;
		new_servicedependency->register_object=TRUE;

		/* add new servicedependency to head of list in memory */
		new_servicedependency->next=xodtemplate_servicedependency_list;
		xodtemplate_servicedependency_list=new_servicedependency;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_servicedependency_list;
		break;

	case XODTEMPLATE_SERVICEESCALATION:

		/* allocate memory */
		new_serviceescalation=(xodtemplate_serviceescalation *)malloc(sizeof(xodtemplate_serviceescalation));
		if(new_serviceescalation==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for serviceescalation definition\n");
#endif
			return ERROR;
		        }
		
		new_serviceescalation->template=NULL;
		new_serviceescalation->name=NULL;
		new_serviceescalation->host_name=NULL;
		new_serviceescalation->service_description=NULL;
		new_serviceescalation->contact_groups=NULL;
		new_serviceescalation->first_notification=-2;
		new_serviceescalation->last_notification=-2;
		new_serviceescalation->notification_interval=-2;
		new_serviceescalation->have_first_notification=FALSE;
		new_serviceescalation->have_last_notification=FALSE;
		new_serviceescalation->have_notification_interval=FALSE;
		new_serviceescalation->has_been_resolved=FALSE;
		new_serviceescalation->register_object=TRUE;

		/* add new serviceescalation to head of list in memory */
		new_serviceescalation->next=xodtemplate_serviceescalation_list;
		xodtemplate_serviceescalation_list=new_serviceescalation;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_serviceescalation_list;
		break;

	case XODTEMPLATE_HOSTGROUPESCALATION:

		/* allocate memory */
		new_hostgroupescalation=(xodtemplate_hostgroupescalation *)malloc(sizeof(xodtemplate_hostgroupescalation));
		if(new_hostgroupescalation==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for hostgroupescalation definition\n");
#endif
			return ERROR;
		        }
		
		new_hostgroupescalation->template=NULL;
		new_hostgroupescalation->name=NULL;
		new_hostgroupescalation->hostgroup_name=NULL;
		new_hostgroupescalation->contact_groups=NULL;
		new_hostgroupescalation->first_notification=-2;
		new_hostgroupescalation->last_notification=-2;
		new_hostgroupescalation->notification_interval=-2;
		new_hostgroupescalation->have_first_notification=FALSE;
		new_hostgroupescalation->have_last_notification=FALSE;
		new_hostgroupescalation->have_notification_interval=FALSE;
		new_hostgroupescalation->has_been_resolved=FALSE;
		new_hostgroupescalation->register_object=TRUE;

		/* add new hostgroupescalation to head of list in memory */
		new_hostgroupescalation->next=xodtemplate_hostgroupescalation_list;
		xodtemplate_hostgroupescalation_list=new_hostgroupescalation;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_hostgroupescalation_list;
		break;

	case XODTEMPLATE_CONTACT:

		/* allocate memory */
		new_contact=(xodtemplate_contact *)malloc(sizeof(xodtemplate_contact));
		if(new_contact==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for contact definition\n");
#endif
			return ERROR;
		        }
		
		new_contact->template=NULL;
		new_contact->name=NULL;
		new_contact->contact_name=NULL;
		new_contact->alias=NULL;
		new_contact->email=NULL;
		new_contact->pager=NULL;
		new_contact->host_notification_period=NULL;
		new_contact->host_notification_commands=NULL;
		new_contact->service_notification_period=NULL;
		new_contact->service_notification_commands=NULL;
		new_contact->notify_on_host_down=FALSE;
		new_contact->notify_on_host_unreachable=FALSE;
		new_contact->notify_on_host_recovery=FALSE;
		new_contact->notify_on_service_unknown=FALSE;
		new_contact->notify_on_service_warning=FALSE;
		new_contact->notify_on_service_critical=FALSE;
		new_contact->notify_on_service_recovery=FALSE;
		new_contact->have_host_notification_options=FALSE;
		new_contact->have_service_notification_options=FALSE;
		new_contact->has_been_resolved=FALSE;
		new_contact->register_object=TRUE;

		/* add new contact to head of list in memory */
		new_contact->next=xodtemplate_contact_list;
		xodtemplate_contact_list=new_contact;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_contact_list;
		break;

	case XODTEMPLATE_HOST:

		/* allocate memory */
		new_host=(xodtemplate_host *)malloc(sizeof(xodtemplate_host));
		if(new_host==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for host definition\n");
#endif
			return ERROR;
		        }
		
		new_host->template=NULL;
		new_host->name=NULL;
		new_host->host_name=NULL;
		new_host->alias=NULL;
		new_host->address=NULL;
		new_host->parents=NULL;
		new_host->check_command=NULL;
		new_host->event_handler=NULL;
		new_host->notification_period=NULL;

		new_host->checks_enabled=TRUE;
		new_host->have_checks_enabled=FALSE;
		new_host->max_check_attempts=-2;
		new_host->have_max_check_attempts=FALSE;
		new_host->event_handler_enabled=TRUE;
		new_host->have_event_handler_enabled=FALSE;
		new_host->flap_detection_enabled=TRUE;
		new_host->have_flap_detection_enabled=FALSE;
		new_host->low_flap_threshold=0.0;
		new_host->have_low_flap_threshold=FALSE;
		new_host->high_flap_threshold=0.0;
		new_host->have_high_flap_threshold=FALSE;
		new_host->notify_on_down=FALSE;
		new_host->notify_on_unreachable=FALSE;
		new_host->notify_on_recovery=FALSE;
		new_host->have_notification_options=FALSE;
		new_host->notifications_enabled=TRUE;
		new_host->have_notifications_enabled=FALSE;
		new_host->notification_interval=-2;
		new_host->have_notification_interval=FALSE;
		new_host->stalk_on_up=FALSE;
		new_host->stalk_on_down=FALSE;
		new_host->stalk_on_unreachable=FALSE;
		new_host->have_stalking_options=FALSE;
		new_host->process_perf_data=TRUE;
		new_host->have_process_perf_data=FALSE;
		new_host->failure_prediction_enabled=TRUE;
		new_host->have_failure_prediction_enabled=FALSE;
		new_host->failure_prediction_options=NULL;
		new_host->retain_status_information=TRUE;
		new_host->have_retain_status_information=FALSE;
		new_host->retain_nonstatus_information=TRUE;
		new_host->have_retain_nonstatus_information=FALSE;
		new_host->has_been_resolved=FALSE;
		new_host->register_object=TRUE;

		/* add new host to head of list in memory */
		new_host->next=xodtemplate_host_list;
		xodtemplate_host_list=new_host;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_host_list;
		break;

	case XODTEMPLATE_SERVICE:

		/* allocate memory */
		new_service=(xodtemplate_service *)malloc(sizeof(xodtemplate_service));
		if(new_service==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for service definition\n");
#endif
			return ERROR;
		        }
		
		new_service->template=NULL;
		new_service->name=NULL;
		new_service->hostgroup_name=NULL;
		new_service->host_name=NULL;
		new_service->service_description=NULL;
		new_service->check_command=NULL;
		new_service->check_period=NULL;
		new_service->event_handler=NULL;
		new_service->notification_period=NULL;
		new_service->contact_groups=NULL;

		new_service->max_check_attempts=-2;
		new_service->have_max_check_attempts=FALSE;
		new_service->normal_check_interval=-2;
		new_service->have_normal_check_interval=FALSE;
		new_service->retry_check_interval=-2;
		new_service->have_retry_check_interval=FALSE;
		new_service->active_checks_enabled=TRUE;
		new_service->have_active_checks_enabled=FALSE;
		new_service->passive_checks_enabled=TRUE;
		new_service->have_passive_checks_enabled=FALSE;
		new_service->parallelize_check=TRUE;
		new_service->have_parallelize_check=FALSE;
		new_service->is_volatile=FALSE;
		new_service->have_is_volatile=FALSE;
		new_service->obsess_over_service=TRUE;
		new_service->have_obsess_over_service=FALSE;
		new_service->event_handler_enabled=TRUE;
		new_service->have_event_handler_enabled=FALSE;
		new_service->check_freshness=FALSE;
		new_service->have_check_freshness=FALSE;
		new_service->freshness_threshold=0;
		new_service->have_freshness_threshold=FALSE;
		new_service->flap_detection_enabled=TRUE;
		new_service->have_flap_detection_enabled=FALSE;
		new_service->low_flap_threshold=0.0;
		new_service->have_low_flap_threshold=FALSE;
		new_service->high_flap_threshold=0.0;
		new_service->have_high_flap_threshold=FALSE;
		new_service->notify_on_unknown=FALSE;
		new_service->notify_on_warning=FALSE;
		new_service->notify_on_critical=FALSE;
		new_service->notify_on_recovery=FALSE;
		new_service->have_notification_options=FALSE;
		new_service->notifications_enabled=TRUE;
		new_service->have_notifications_enabled=FALSE;
		new_service->notification_interval=-2;
		new_service->have_notification_interval=FALSE;
		new_service->stalk_on_ok=FALSE;
		new_service->stalk_on_unknown=FALSE;
		new_service->stalk_on_warning=FALSE;
		new_service->stalk_on_critical=FALSE;
		new_service->have_stalking_options=FALSE;
		new_service->process_perf_data=TRUE;
		new_service->have_process_perf_data=FALSE;
		new_service->failure_prediction_enabled=TRUE;
		new_service->have_failure_prediction_enabled=FALSE;
		new_service->failure_prediction_options=NULL;
		new_service->retain_status_information=TRUE;
		new_service->have_retain_status_information=FALSE;
		new_service->retain_nonstatus_information=TRUE;
		new_service->have_retain_nonstatus_information=FALSE;
		new_service->has_been_resolved=FALSE;
		new_service->register_object=TRUE;

		/* add new service to head of list in memory */
		new_service->next=xodtemplate_service_list;
		xodtemplate_service_list=new_service;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_service_list;
		break;

	case XODTEMPLATE_HOSTDEPENDENCY:

		/* allocate memory */
		new_hostdependency=(xodtemplate_hostdependency *)malloc(sizeof(xodtemplate_hostdependency));
		if(new_hostdependency==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for hostdependency definition\n");
#endif
			return ERROR;
		        }
		
		new_hostdependency->template=NULL;
		new_hostdependency->name=NULL;
		new_hostdependency->host_name=NULL;
		new_hostdependency->dependent_host_name=NULL;
		new_hostdependency->fail_notify_on_up=FALSE;
		new_hostdependency->fail_notify_on_down=FALSE;
		new_hostdependency->fail_notify_on_unreachable=FALSE;
		new_hostdependency->have_notification_dependency_options=FALSE;
		new_hostdependency->has_been_resolved=FALSE;
		new_hostdependency->register_object=TRUE;

		/* add new hostdependency to head of list in memory */
		new_hostdependency->next=xodtemplate_hostdependency_list;
		xodtemplate_hostdependency_list=new_hostdependency;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_hostdependency_list;
		break;

	case XODTEMPLATE_HOSTESCALATION:

		/* allocate memory */
		new_hostescalation=(xodtemplate_hostescalation *)malloc(sizeof(xodtemplate_hostescalation));
		if(new_hostescalation==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for hostescalation definition\n");
#endif
			return ERROR;
		        }
		
		new_hostescalation->template=NULL;
		new_hostescalation->name=NULL;
		new_hostescalation->hostgroup_name=NULL;
		new_hostescalation->host_name=NULL;
		new_hostescalation->contact_groups=NULL;
		new_hostescalation->first_notification=-2;
		new_hostescalation->last_notification=-2;
		new_hostescalation->notification_interval=-2;
		new_hostescalation->have_first_notification=FALSE;
		new_hostescalation->have_last_notification=FALSE;
		new_hostescalation->have_notification_interval=FALSE;
		new_hostescalation->has_been_resolved=FALSE;
		new_hostescalation->register_object=TRUE;

		/* add new hostescalation to head of list in memory */
		new_hostescalation->next=xodtemplate_hostescalation_list;
		xodtemplate_hostescalation_list=new_hostescalation;

		/* update current object pointer */
		xodtemplate_current_object=xodtemplate_hostescalation_list;
		break;

	default:
		return ERROR;
		break;
	        }

#ifdef DEBUG0
	printf("xodtemplate_begin_object_definition() end\n");
#endif

	return result;
        }



/* adds a property to an object definition */
int xodtemplate_add_object_property(char *input, int options){
	int result=OK;
	char variable[MAX_XODTEMPLATE_INPUT_BUFFER];
	char value[MAX_XODTEMPLATE_INPUT_BUFFER];
	char *temp_ptr;
	xodtemplate_timeperiod *temp_timeperiod;
	xodtemplate_command *temp_command;
	xodtemplate_contactgroup *temp_contactgroup;
	xodtemplate_hostgroup *temp_hostgroup;
	xodtemplate_servicedependency *temp_servicedependency;
	xodtemplate_serviceescalation *temp_serviceescalation;
	xodtemplate_hostgroupescalation *temp_hostgroupescalation;
	xodtemplate_contact *temp_contact;
	xodtemplate_host *temp_host;
	xodtemplate_service *temp_service;
	xodtemplate_hostdependency *temp_hostdependency;
	xodtemplate_hostescalation *temp_hostescalation;
	register int x;
	register int y;

#ifdef DEBUG0
	printf("xodtemplate_add_object_property() start\n");
#endif


	/* check to see if we should process this type of object */
	switch(xodtemplate_current_object_type){
	case XODTEMPLATE_TIMEPERIOD:
		if(!(options & READ_TIMEPERIODS))
			return OK;
		break;
	case XODTEMPLATE_COMMAND:
		if(!(options & READ_COMMANDS))
			return OK;
		break;
	case XODTEMPLATE_CONTACT:
		if(!(options & READ_CONTACTS))
			return OK;
		break;
	case XODTEMPLATE_CONTACTGROUP:
		if(!(options & READ_CONTACTGROUPS))
			return OK;
		break;
	case XODTEMPLATE_HOST:
		if(!(options & READ_HOSTS))
			return OK;
		break;
	case XODTEMPLATE_HOSTGROUP:
		if(!(options & READ_HOSTGROUPS))
			return OK;
		break;
	case XODTEMPLATE_SERVICE:
		if(!(options & READ_SERVICES))
			return OK;
		break;
	case XODTEMPLATE_SERVICEDEPENDENCY:
		if(!(options & READ_SERVICEDEPENDENCIES))
			return OK;
		break;
	case XODTEMPLATE_SERVICEESCALATION:
		if(!(options & READ_SERVICEESCALATIONS))
			return OK;
		break;
	case XODTEMPLATE_HOSTGROUPESCALATION:
		if(!(options & READ_HOSTGROUPESCALATIONS))
			return OK;
		break;
	case XODTEMPLATE_HOSTDEPENDENCY:
		if(!(options & READ_HOSTDEPENDENCIES))
			return OK;
		break;
	case XODTEMPLATE_HOSTESCALATION:
		if(!(options & READ_HOSTESCALATIONS))
			return OK;
		break;
	default:
		return ERROR;
		break;
	        }



	/* truncate if necessary */
	if(strlen(input)>MAX_XODTEMPLATE_INPUT_BUFFER)
		input[MAX_XODTEMPLATE_INPUT_BUFFER-1]='\x0';

	/* get variable name */
	for(x=0,y=0;input[x]!='\x0';x++){
		if(input[x]==' ' || input[x]=='\t')
			break;
		else
			variable[y++]=input[x];
	        }
	variable[y]='\x0';
			
	/* get variable value */
	if(x>=strlen(input)){
#ifdef NSCORE
		printf("Error: NULL variable value in object definition.\n");
#endif
		return ERROR;
	        }
	for(y=0;input[x]!='\x0';x++)
		value[y++]=input[x];
	value[y]='\x0';

	/*
	printf("RAW VARIABLE: '%s'\n",variable);
	printf("RAW VALUE: '%s'\n",value);
	*/

#ifdef RUN_SLOW_AS_HELL
	xodtemplate_strip(variable);
#endif
	xodtemplate_strip(value);

	/*
	printf("STRIPPED VARIABLE: '%s'\n",variable);
	printf("STRIPPED VALUE: '%s'\n\n",value);
	*/


	switch(xodtemplate_current_object_type){

	case XODTEMPLATE_TIMEPERIOD:
		
		temp_timeperiod=(xodtemplate_timeperiod *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_timeperiod->template=(char *)malloc(strlen(value)+1);
			if(temp_timeperiod->template==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for timeperiod template.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_timeperiod->template,value);
		        }
		else if(!strcmp(variable,"name")){
			temp_timeperiod->name=(char *)malloc(strlen(value)+1);
			if(temp_timeperiod->name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for timeperiod name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_timeperiod->name,value);
		        }
		else if(!strcmp(variable,"timeperiod_name")){
			temp_timeperiod->timeperiod_name=(char *)malloc(strlen(value)+1);
			if(temp_timeperiod->timeperiod_name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for timeperiod name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_timeperiod->timeperiod_name,value);
		        }
		else if(!strcmp(variable,"alias")){
			temp_timeperiod->alias=(char *)malloc(strlen(value)+1);
			if(temp_timeperiod->alias==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for timeperiod alias.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_timeperiod->alias,value);
		        }
		else if(!strcmp(variable,"monday") || !strcmp(variable,"tuesday") || !strcmp(variable,"wednesday") || !strcmp(variable,"thursday") || !strcmp(variable,"friday") || !strcmp(variable,"saturday") || !strcmp(variable,"sunday")){
			if(!strcmp(variable,"monday"))
				x=1;
			else if(!strcmp(variable,"tuesday"))
				x=2;
			else if(!strcmp(variable,"wednesday"))
				x=3;
			else if(!strcmp(variable,"thursday"))
				x=4;
			else if(!strcmp(variable,"friday"))
				x=5;
			else if(!strcmp(variable,"saturday"))
				x=6;
			else
				x=0;
			temp_timeperiod->timeranges[x]=(char *)malloc(strlen(value)+1);
			if(temp_timeperiod->timeranges[x]==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for timeperiod timerange.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_timeperiod->timeranges[x],value);
		        }
		else if(!strcmp(variable,"register"))
			temp_timeperiod->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			printf("Error: Invalid timeperiod object directive '%s'.\n",variable);
#endif
			return ERROR;
		        }
		break;



	case XODTEMPLATE_COMMAND:

		temp_command=(xodtemplate_command *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_command->template=(char *)malloc(strlen(value)+1);
			if(temp_command->template==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for command template.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_command->template,value);
		        }
		else if(!strcmp(variable,"name")){
			temp_command->name=(char *)malloc(strlen(value)+1);
			if(temp_command->name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for command name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_command->name,value);
		        }
		else if(!strcmp(variable,"command_name")){
			temp_command->command_name=(char *)malloc(strlen(value)+1);
			if(temp_command->command_name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for command name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_command->command_name,value);
		        }
		else if(!strcmp(variable,"command_line")){
			temp_command->command_line=(char *)malloc(strlen(value)+1);
			if(temp_command->command_line==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for command line.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_command->command_line,value);
		        }
		else if(!strcmp(variable,"register"))
			temp_command->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			printf("Error: Invalid command object directive '%s'.\n",variable);
#endif
			return ERROR;
		        }

		break;

	case XODTEMPLATE_CONTACTGROUP:

		temp_contactgroup=(xodtemplate_contactgroup *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_contactgroup->template=(char *)malloc(strlen(value)+1);
			if(temp_contactgroup->template==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contactgroup template.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contactgroup->template,value);
		        }
		else if(!strcmp(variable,"name")){
			temp_contactgroup->name=(char *)malloc(strlen(value)+1);
			if(temp_contactgroup->name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contactgroup name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contactgroup->name,value);
		        }
		else if(!strcmp(variable,"contactgroup_name")){
			temp_contactgroup->contactgroup_name=(char *)malloc(strlen(value)+1);
			if(temp_contactgroup->contactgroup_name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contactgroup name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contactgroup->contactgroup_name,value);
		        }
		else if(!strcmp(variable,"alias")){
			temp_contactgroup->alias=(char *)malloc(strlen(value)+1);
			if(temp_contactgroup->alias==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contactgroup alias.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contactgroup->alias,value);
		        }
		else if(!strcmp(variable,"members")){
			temp_contactgroup->members=(char *)malloc(strlen(value)+1);
			if(temp_contactgroup->members==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contactgroup members.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contactgroup->members,value);
		        }
		else if(!strcmp(variable,"register"))
			temp_contactgroup->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			printf("Error: Invalid contactgroup object directive '%s'.\n",variable);
#endif
			return ERROR;
		        }

		break;

	case XODTEMPLATE_HOSTGROUP:

		temp_hostgroup=(xodtemplate_hostgroup *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_hostgroup->template=(char *)malloc(strlen(value)+1);
			if(temp_hostgroup->template==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostgroup template.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostgroup->template,value);
		        }
		else if(!strcmp(variable,"name")){
			temp_hostgroup->name=(char *)malloc(strlen(value)+1);
			if(temp_hostgroup->name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostgroup name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostgroup->name,value);
		        }
		else if(!strcmp(variable,"hostgroup_name")){
			temp_hostgroup->hostgroup_name=(char *)malloc(strlen(value)+1);
			if(temp_hostgroup->hostgroup_name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostgroup name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostgroup->hostgroup_name,value);
		        }
		else if(!strcmp(variable,"alias")){
			temp_hostgroup->alias=(char *)malloc(strlen(value)+1);
			if(temp_hostgroup->alias==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostgroup alias.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostgroup->alias,value);
		        }
		else if(!strcmp(variable,"members")){
			temp_hostgroup->members=(char *)malloc(strlen(value)+1);
			if(temp_hostgroup->members==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostgroup members.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostgroup->members,value);
		        }
		else if(!strcmp(variable,"contact_groups")){
			temp_hostgroup->contact_groups=(char *)malloc(strlen(value)+1);
			if(temp_hostgroup->contact_groups==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostgroup contact_groups.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostgroup->contact_groups,value);
		        }
		else if(!strcmp(variable,"register"))
			temp_hostgroup->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			printf("Error: Invalid hostgroup object directive '%s'.\n",variable);
#endif
			return ERROR;
		        }

		break;

	
	case XODTEMPLATE_SERVICEDEPENDENCY:

		temp_servicedependency=(xodtemplate_servicedependency *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_servicedependency->template=(char *)malloc(strlen(value)+1);
			if(temp_servicedependency->template==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for servicedependency template.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_servicedependency->template,value);
		        }
		else if(!strcmp(variable,"name")){
			temp_servicedependency->name=(char *)malloc(strlen(value)+1);
			if(temp_servicedependency->name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for servicedependency name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_servicedependency->name,value);
		        }
		else if(!strcmp(variable,"host") || !strcmp(variable,"host_name") || !strcmp(variable,"master_host") || !strcmp(variable,"master_host_name")){
			temp_servicedependency->host_name=(char *)malloc(strlen(value)+1);
			if(temp_servicedependency->host_name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for servicedependency host.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_servicedependency->host_name,value);
		        }
		else if(!strcmp(variable,"description") || !strcmp(variable,"service_description") || !strcmp(variable,"master_description") || !strcmp(variable,"master_service_description")){
			temp_servicedependency->service_description=(char *)malloc(strlen(value)+1);
			if(temp_servicedependency->service_description==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for servicedependency service_description.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_servicedependency->service_description,value);
		        }
		else if(!strcmp(variable,"dependent_host") || !strcmp(variable,"dependent_host_name")){
			temp_servicedependency->dependent_host_name=(char *)malloc(strlen(value)+1);
			if(temp_servicedependency->dependent_host_name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for servicedependency dependent host.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_servicedependency->dependent_host_name,value);
		        }
		else if(!strcmp(variable,"dependent_description") || !strcmp(variable,"dependent_service_description")){
			temp_servicedependency->dependent_service_description=(char *)malloc(strlen(value)+1);
			if(temp_servicedependency->dependent_service_description==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for servicedependency dependent service_description.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_servicedependency->dependent_service_description,value);
		        }
		else if(!strcmp(variable,"execution_failure_options") || !strcmp(variable,"execution_failure_criteria")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"o") || !strcmp(temp_ptr,"ok"))
					temp_servicedependency->fail_execute_on_ok=TRUE;
				else if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unknown"))
					temp_servicedependency->fail_execute_on_unknown=TRUE;
				else if(!strcmp(temp_ptr,"w") || !strcmp(temp_ptr,"warning"))
					temp_servicedependency->fail_execute_on_warning=TRUE;
				else if(!strcmp(temp_ptr,"c") || !strcmp(temp_ptr,"critical"))
					temp_servicedependency->fail_execute_on_critical=TRUE;
				else if(!strcmp(temp_ptr,"n") || !strcmp(temp_ptr,"none")){
					temp_servicedependency->fail_execute_on_ok=FALSE;
					temp_servicedependency->fail_execute_on_unknown=FALSE;
					temp_servicedependency->fail_execute_on_warning=FALSE;
					temp_servicedependency->fail_execute_on_critical=FALSE;
				        }
				else{
#ifdef NSCORE
					printf("Error: Invalid execution dependency option '%s' in servicedependency definition.\n",temp_ptr);
#endif
					return ERROR;
				        }
			        }
			temp_servicedependency->have_execution_dependency_options=TRUE;
		        }
		else if(!strcmp(variable,"notification_failure_options") || !strcmp(variable,"notification_failure_criteria")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"o") || !strcmp(temp_ptr,"ok"))
					temp_servicedependency->fail_notify_on_ok=TRUE;
				else if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unknown"))
					temp_servicedependency->fail_notify_on_unknown=TRUE;
				else if(!strcmp(temp_ptr,"w") || !strcmp(temp_ptr,"warning"))
					temp_servicedependency->fail_notify_on_warning=TRUE;
				else if(!strcmp(temp_ptr,"c") || !strcmp(temp_ptr,"critical"))
					temp_servicedependency->fail_notify_on_critical=TRUE;
				else if(!strcmp(temp_ptr,"n") || !strcmp(temp_ptr,"none")){
					temp_servicedependency->fail_notify_on_ok=FALSE;
					temp_servicedependency->fail_notify_on_unknown=FALSE;
					temp_servicedependency->fail_notify_on_warning=FALSE;
					temp_servicedependency->fail_notify_on_critical=FALSE;
				        }
				else{
#ifdef NSCORE
					printf("Error: Invalid notification dependency option '%s' in servicedependency definition.\n",temp_ptr);
#endif
					return ERROR;
				        }
			        }
			temp_servicedependency->have_notification_dependency_options=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_servicedependency->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			printf("Error: Invalid servicedependency object directive '%s'.\n",variable);
#endif
			return ERROR;
		        }

		break;

	
	case XODTEMPLATE_SERVICEESCALATION:

		temp_serviceescalation=(xodtemplate_serviceescalation *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_serviceescalation->template=(char *)malloc(strlen(value)+1);
			if(temp_serviceescalation->template==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for serviceescalation template.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_serviceescalation->template,value);
		        }
		else if(!strcmp(variable,"name")){
			temp_serviceescalation->name=(char *)malloc(strlen(value)+1);
			if(temp_serviceescalation->name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for serviceescalation name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_serviceescalation->name,value);
		        }
		else if(!strcmp(variable,"host") || !strcmp(variable,"host_name")){
			temp_serviceescalation->host_name=(char *)malloc(strlen(value)+1);
			if(temp_serviceescalation->host_name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for serviceescalation host.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_serviceescalation->host_name,value);
		        }
		else if(!strcmp(variable,"description") || !strcmp(variable,"service_description")){
			temp_serviceescalation->service_description=(char *)malloc(strlen(value)+1);
			if(temp_serviceescalation->service_description==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for serviceescalation service_description.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_serviceescalation->service_description,value);
		        }
		else if(!strcmp(variable,"contact_groups")){
			temp_serviceescalation->contact_groups=(char *)malloc(strlen(value)+1);
			if(temp_serviceescalation->contact_groups==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for serviceescalation contact_groups.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_serviceescalation->contact_groups,value);
		        }
		else if(!strcmp(variable,"first_notification")){
			temp_serviceescalation->first_notification=atoi(value);
			temp_serviceescalation->have_first_notification=TRUE;
		        }
		else if(!strcmp(variable,"last_notification")){
			temp_serviceescalation->last_notification=atoi(value);
			temp_serviceescalation->have_last_notification=TRUE;
		        }
		else if(!strcmp(variable,"notification_interval")){
			temp_serviceescalation->notification_interval=atoi(value);
			temp_serviceescalation->have_notification_interval=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_serviceescalation->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			printf("Error: Invalid servicedeescalation object directive '%s'.\n",variable);
#endif
			return ERROR;
		        }

		break;
	

	case XODTEMPLATE_HOSTGROUPESCALATION:

		temp_hostgroupescalation=(xodtemplate_hostgroupescalation *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_hostgroupescalation->template=(char *)malloc(strlen(value)+1);
			if(temp_hostgroupescalation->template==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostgroupescalation template.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostgroupescalation->template,value);
		        }
		else if(!strcmp(variable,"name")){
			temp_hostgroupescalation->name=(char *)malloc(strlen(value)+1);
			if(temp_hostgroupescalation->name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostgroupescalation name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostgroupescalation->name,value);
		        }
		else if(!strcmp(variable,"hostgroup") || !strcmp(variable,"hostgroups") || !strcmp(variable,"hostgroup_name")){
			temp_hostgroupescalation->hostgroup_name=(char *)malloc(strlen(value)+1);
			if(temp_hostgroupescalation->hostgroup_name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostgroupescalation hostgroup.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostgroupescalation->hostgroup_name,value);
		        }
		else if(!strcmp(variable,"contact_groups")){
			temp_hostgroupescalation->contact_groups=(char *)malloc(strlen(value)+1);
			if(temp_hostgroupescalation->contact_groups==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostgroupescalation contact_groups.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostgroupescalation->contact_groups,value);
		        }
		else if(!strcmp(variable,"first_notification")){
			temp_hostgroupescalation->first_notification=atoi(value);
			temp_hostgroupescalation->have_first_notification=TRUE;
		        }
		else if(!strcmp(variable,"last_notification")){
			temp_hostgroupescalation->last_notification=atoi(value);
			temp_hostgroupescalation->have_last_notification=TRUE;
		        }
		else if(!strcmp(variable,"notification_interval")){
			temp_hostgroupescalation->notification_interval=atoi(value);
			temp_hostgroupescalation->have_notification_interval=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_hostgroupescalation->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			printf("Error: Invalid hostgroupescalation object directive '%s'.\n",variable);
#endif
			return ERROR;
		        }

		break;
	

	case XODTEMPLATE_CONTACT:

		temp_contact=(xodtemplate_contact *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_contact->template=(char *)malloc(strlen(value)+1);
			if(temp_contact->template==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contact template.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contact->template,value);
		        }
		else if(!strcmp(variable,"name")){
			temp_contact->name=(char *)malloc(strlen(value)+1);
			if(temp_contact->name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contact name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contact->name,value);
		        }
		else if(!strcmp(variable,"contact_name")){
			temp_contact->contact_name=(char *)malloc(strlen(value)+1);
			if(temp_contact->contact_name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contact name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contact->contact_name,value);
		        }
		else if(!strcmp(variable,"alias")){
			temp_contact->alias=(char *)malloc(strlen(value)+1);
			if(temp_contact->alias==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contact alias.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contact->alias,value);
		        }
		else if(!strcmp(variable,"email")){
			temp_contact->email=(char *)malloc(strlen(value)+1);
			if(temp_contact->email==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contact email.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contact->email,value);
		        }
		else if(!strcmp(variable,"pager")){
			temp_contact->pager=(char *)malloc(strlen(value)+1);
			if(temp_contact->pager==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contact pager.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contact->pager,value);
		        }
		else if(!strcmp(variable,"host_notification_period")){
			temp_contact->host_notification_period=(char *)malloc(strlen(value)+1);
			if(temp_contact->host_notification_period==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contact host_notification_period.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contact->host_notification_period,value);
		        }
		else if(!strcmp(variable,"host_notification_commands")){
			temp_contact->host_notification_commands=(char *)malloc(strlen(value)+1);
			if(temp_contact->host_notification_commands==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contact host_notification_commands.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contact->host_notification_commands,value);
		        }
		else if(!strcmp(variable,"service_notification_period")){
			temp_contact->service_notification_period=(char *)malloc(strlen(value)+1);
			if(temp_contact->service_notification_period==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contact service_notification_period.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contact->service_notification_period,value);
		        }
		else if(!strcmp(variable,"service_notification_commands")){
			temp_contact->service_notification_commands=(char *)malloc(strlen(value)+1);
			if(temp_contact->service_notification_commands==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contact service_notification_commands.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contact->service_notification_commands,value);
		        }
		else if(!strcmp(variable,"host_notification_period")){
			temp_contact->host_notification_period=(char *)malloc(strlen(value)+1);
			if(temp_contact->host_notification_period==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contact host_notification_period.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contact->host_notification_period,value);
		        }
		else if(!strcmp(variable,"service_notification_period")){
			temp_contact->service_notification_period=(char *)malloc(strlen(value)+1);
			if(temp_contact->service_notification_period==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for contact service_notification_period.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_contact->service_notification_period,value);
		        }
		else if(!strcmp(variable,"host_notification_options")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"d") || !strcmp(temp_ptr,"down"))
					temp_contact->notify_on_host_down=TRUE;
				else if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unreachable"))
					temp_contact->notify_on_host_unreachable=TRUE;
				else if(!strcmp(temp_ptr,"r") || !strcmp(temp_ptr,"recovery"))
					temp_contact->notify_on_host_recovery=TRUE;
				else if(!strcmp(temp_ptr,"n") || !strcmp(temp_ptr,"none")){
					temp_contact->notify_on_host_down=FALSE;
					temp_contact->notify_on_host_unreachable=FALSE;
					temp_contact->notify_on_host_recovery=FALSE;
				        }
				else{
#ifdef NSCORE
					printf("Error: Invalid host notification option '%s' in contact definition.\n",temp_ptr);
#endif
					return ERROR;
				        }
			        }
			temp_contact->have_host_notification_options=TRUE;
		        }
		else if(!strcmp(variable,"service_notification_options")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unknown"))
					temp_contact->notify_on_service_unknown=TRUE;
				else if(!strcmp(temp_ptr,"w") || !strcmp(temp_ptr,"warning"))
					temp_contact->notify_on_service_warning=TRUE;
				else if(!strcmp(temp_ptr,"c") || !strcmp(temp_ptr,"critical"))
					temp_contact->notify_on_service_critical=TRUE;
				else if(!strcmp(temp_ptr,"r") || !strcmp(temp_ptr,"recovery"))
					temp_contact->notify_on_service_recovery=TRUE;
				else if(!strcmp(temp_ptr,"n") || !strcmp(temp_ptr,"none")){
					temp_contact->notify_on_service_unknown=FALSE;
					temp_contact->notify_on_service_warning=FALSE;
					temp_contact->notify_on_service_critical=FALSE;
					temp_contact->notify_on_service_recovery=FALSE;
				        }
				else{
#ifdef NSCORE
					printf("Error: Invalid service notification option '%s' in contact definition.\n",temp_ptr);
#endif
					return ERROR;
				        }
			        }
			temp_contact->have_service_notification_options=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_contact->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			printf("Error: Invalid contact object directive '%s'.\n",variable);
#endif
			return ERROR;
		        }

		break;


	case XODTEMPLATE_HOST:

		temp_host=(xodtemplate_host *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_host->template=(char *)malloc(strlen(value)+1);
			if(temp_host->template==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for host template.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_host->template,value);
		        }
		else if(!strcmp(variable,"name")){
			temp_host->name=(char *)malloc(strlen(value)+1);
			if(temp_host->name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for host name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_host->name,value);
		        }
		else if(!strcmp(variable,"host_name")){
			temp_host->host_name=(char *)malloc(strlen(value)+1);
			if(temp_host->host_name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for host name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_host->host_name,value);
		        }
		else if(!strcmp(variable,"alias")){
			temp_host->alias=(char *)malloc(strlen(value)+1);
			if(temp_host->alias==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for host alias.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_host->alias,value);
		        }
		else if(!strcmp(variable,"address")){
			temp_host->address=(char *)malloc(strlen(value)+1);
			if(temp_host->address==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for host address.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_host->address,value);
		        }
		else if(!strcmp(variable,"parents")){
			temp_host->parents=(char *)malloc(strlen(value)+1);
			if(temp_host->parents==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for host parents.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_host->parents,value);
		        }
		else if(!strcmp(variable,"notification_period")){
			temp_host->notification_period=(char *)malloc(strlen(value)+1);
			if(temp_host->notification_period==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for host notification_period.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_host->notification_period,value);
		        }
		else if(!strcmp(variable,"check_command")){
			temp_host->check_command=(char *)malloc(strlen(value)+1);
			if(temp_host->check_command==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for host check_command.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_host->check_command,value);
		        }
		else if(!strcmp(variable,"event_handler")){
			temp_host->event_handler=(char *)malloc(strlen(value)+1);
			if(temp_host->event_handler==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for host event_handler.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_host->event_handler,value);
		        }
		else if(!strcmp(variable,"failure_prediction_options")){
			temp_host->failure_prediction_options=(char *)malloc(strlen(value)+1);
			if(temp_host->failure_prediction_options==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for host failure_prediction_options.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_host->failure_prediction_options,value);
		        }
		else if(!strcmp(variable,"max_check_attempts")){
			temp_host->max_check_attempts=atoi(value);
			temp_host->have_max_check_attempts=TRUE;
		        }
		else if(!strcmp(variable,"checks_enabled")){
			temp_host->checks_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_checks_enabled=TRUE;
		        }
		else if(!strcmp(variable,"event_handler_enabled")){
			temp_host->event_handler_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_event_handler_enabled=TRUE;
		        }
		else if(!strcmp(variable,"low_flap_threshold")){
			temp_host->low_flap_threshold=strtod(value,NULL);
			temp_host->have_low_flap_threshold=TRUE;
		        }
		else if(!strcmp(variable,"high_flap_threshold")){
			temp_host->high_flap_threshold=strtod(value,NULL);
			temp_host->have_high_flap_threshold=TRUE;
		        }
		else if(!strcmp(variable,"flap_detection_enabled")){
			temp_host->flap_detection_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_flap_detection_enabled=TRUE;
		        }
		else if(!strcmp(variable,"notification_options")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"d") || !strcmp(temp_ptr,"down"))
					temp_host->notify_on_down=TRUE;
				else if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unreachable"))
					temp_host->notify_on_unreachable=TRUE;
				else if(!strcmp(temp_ptr,"r") || !strcmp(temp_ptr,"recovery"))
					temp_host->notify_on_recovery=TRUE;
				else if(!strcmp(temp_ptr,"n") || !strcmp(temp_ptr,"none")){
					temp_host->notify_on_down=FALSE;
					temp_host->notify_on_unreachable=FALSE;
					temp_host->notify_on_recovery=FALSE;
				        }
				else{
#ifdef NSCORE
					printf("Error: Invalid notification option '%s' in host definition.\n",temp_ptr);
#endif
					return ERROR;
				        }
			        }
			temp_host->have_notification_options=TRUE;
		        }
		else if(!strcmp(variable,"notifications_enabled")){
			temp_host->notifications_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_notifications_enabled=TRUE;
		        }
		else if(!strcmp(variable,"notification_interval")){
			temp_host->notification_interval=atoi(value);
			temp_host->have_notification_interval=TRUE;
		        }
		else if(!strcmp(variable,"stalking_options")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"o") || !strcmp(temp_ptr,"up"))
					temp_host->stalk_on_up=TRUE;
				else if(!strcmp(temp_ptr,"d") || !strcmp(temp_ptr,"down"))
					temp_host->stalk_on_down=TRUE;
				else if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unreachable"))
					temp_host->stalk_on_unreachable=TRUE;
				else{
#ifdef NSCORE
					printf("Error: Invalid stalking option '%s' in host definition.\n",temp_ptr);
#endif
					return ERROR;
				        }
			        }
			temp_host->have_stalking_options=TRUE;
		        }
		else if(!strcmp(variable,"process_perf_data")){
			temp_host->process_perf_data=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_process_perf_data=TRUE;
		        }
		else if(!strcmp(variable,"failure_prediction_enabled")){
			temp_host->failure_prediction_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_failure_prediction_enabled=TRUE;
		        }
		else if(!strcmp(variable,"retain_status_information")){
			temp_host->retain_status_information=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_retain_status_information=TRUE;
		        }
		else if(!strcmp(variable,"retain_nonstatus_information")){
			temp_host->retain_nonstatus_information=(atoi(value)>0)?TRUE:FALSE;
			temp_host->have_retain_nonstatus_information=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_host->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			printf("Error: Invalid host object directive '%s'.\n",variable);
#endif
			return ERROR;
		        }

		break;

	case XODTEMPLATE_SERVICE:

		temp_service=(xodtemplate_service *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_service->template=(char *)malloc(strlen(value)+1);
			if(temp_service->template==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for service template.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_service->template,value);
		        }
		else if(!strcmp(variable,"name")){
			temp_service->name=(char *)malloc(strlen(value)+1);
			if(temp_service->name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for service name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_service->name,value);
		        }
		else if(!strcmp(variable,"hostgroup") || !strcmp(variable,"hostgroups") || !strcmp(variable,"hostgroup_name")){
			temp_service->hostgroup_name=(char *)malloc(strlen(value)+1);
			if(temp_service->hostgroup_name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for service hostgroup.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_service->hostgroup_name,value);
		        }
		else if(!strcmp(variable,"host") || !strcmp(variable,"hosts") || !strcmp(variable,"host_name")){
			temp_service->host_name=(char *)malloc(strlen(value)+1);
			if(temp_service->host_name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for service host.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_service->host_name,value);
		        }
		else if(!strcmp(variable,"service_description") || !strcmp(variable,"description")){
			temp_service->service_description=(char *)malloc(strlen(value)+1);
			if(temp_service->service_description==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for service service_description.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_service->service_description,value);
		        }
		else if(!strcmp(variable,"check_command")){
			temp_service->check_command=(char *)malloc(strlen(value)+1);
			if(temp_service->check_command==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for service check_command.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_service->check_command,value);
		        }
		else if(!strcmp(variable,"check_period")){
			temp_service->check_period=(char *)malloc(strlen(value)+1);
			if(temp_service->check_period==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for service check_period.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_service->check_period,value);
		        }
		else if(!strcmp(variable,"event_handler")){
			temp_service->event_handler=(char *)malloc(strlen(value)+1);
			if(temp_service->event_handler==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for service event_handler.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_service->event_handler,value);
		        }
		else if(!strcmp(variable,"notification_period")){
			temp_service->notification_period=(char *)malloc(strlen(value)+1);
			if(temp_service->notification_period==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for service notification_period.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_service->notification_period,value);
		        }
		else if(!strcmp(variable,"contact_groups")){
			temp_service->contact_groups=(char *)malloc(strlen(value)+1);
			if(temp_service->contact_groups==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for service contact_groups.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_service->contact_groups,value);
		        }
		else if(!strcmp(variable,"failure_prediction_options")){
			temp_service->failure_prediction_options=(char *)malloc(strlen(value)+1);
			if(temp_service->failure_prediction_options==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for service failure_prediction_options.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_service->failure_prediction_options,value);
		        }
		else if(!strcmp(variable,"max_check_attempts")){
			temp_service->max_check_attempts=atoi(value);
			temp_service->have_max_check_attempts=TRUE;
		        }
		else if(!strcmp(variable,"normal_check_interval")){
			temp_service->normal_check_interval=atoi(value);
			temp_service->have_normal_check_interval=TRUE;
		        }
		else if(!strcmp(variable,"retry_check_interval")){
			temp_service->retry_check_interval=atoi(value);
			temp_service->have_retry_check_interval=TRUE;
		        }
		else if(!strcmp(variable,"active_checks_enabled")){
			temp_service->active_checks_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_active_checks_enabled=TRUE;
		        }
		else if(!strcmp(variable,"passive_checks_enabled")){
			temp_service->passive_checks_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_passive_checks_enabled=TRUE;
		        }
		else if(!strcmp(variable,"parallelize_check")){
			temp_service->parallelize_check=atoi(value);
			temp_service->have_parallelize_check=TRUE;
		        }
		else if(!strcmp(variable,"is_volatile")){
			temp_service->is_volatile=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_is_volatile=TRUE;
		        }
		else if(!strcmp(variable,"obsess_over_service")){
			temp_service->obsess_over_service=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_obsess_over_service=TRUE;
		        }
		else if(!strcmp(variable,"event_handler_enabled")){
			temp_service->event_handler_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_event_handler_enabled=TRUE;
		        }
		else if(!strcmp(variable,"check_freshness")){
			temp_service->check_freshness=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_check_freshness=TRUE;
		        }
		else if(!strcmp(variable,"freshness_threshold")){
			temp_service->freshness_threshold=atoi(value);
			temp_service->have_freshness_threshold=TRUE;
		        }
		else if(!strcmp(variable,"low_flap_threshold")){
			temp_service->low_flap_threshold=strtod(value,NULL);
			temp_service->have_low_flap_threshold=TRUE;
		        }
		else if(!strcmp(variable,"high_flap_threshold")){
			temp_service->high_flap_threshold=strtod(value,NULL);
			temp_service->have_high_flap_threshold=TRUE;
		        }
		else if(!strcmp(variable,"flap_detection_enabled")){
			temp_service->flap_detection_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_flap_detection_enabled=TRUE;
		        }
		else if(!strcmp(variable,"notification_options")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unknown"))
					temp_service->notify_on_unknown=TRUE;
				else if(!strcmp(temp_ptr,"w") || !strcmp(temp_ptr,"warning"))
					temp_service->notify_on_warning=TRUE;
				else if(!strcmp(temp_ptr,"c") || !strcmp(temp_ptr,"critical"))
					temp_service->notify_on_critical=TRUE;
				else if(!strcmp(temp_ptr,"r") || !strcmp(temp_ptr,"recovery"))
					temp_service->notify_on_recovery=TRUE;
				else if(!strcmp(temp_ptr,"n") || !strcmp(temp_ptr,"none")){
					temp_service->notify_on_unknown=FALSE;
					temp_service->notify_on_warning=FALSE;
					temp_service->notify_on_critical=FALSE;
					temp_service->notify_on_recovery=FALSE;
				        }
				else{
#ifdef NSCORE
					printf("Error: Invalid notification option '%s' in service definition.\n",temp_ptr);
#endif
					return ERROR;
				        }
			        }
			temp_service->have_notification_options=TRUE;
		        }
		else if(!strcmp(variable,"notifications_enabled")){
			temp_service->notifications_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_notifications_enabled=TRUE;
		        }
		else if(!strcmp(variable,"notification_interval")){
			temp_service->notification_interval=atoi(value);
			temp_service->have_notification_interval=TRUE;
		        }
		else if(!strcmp(variable,"stalking_options")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"o") || !strcmp(temp_ptr,"ok"))
					temp_service->stalk_on_ok=TRUE;
				else if(!strcmp(temp_ptr,"w") || !strcmp(temp_ptr,"warning"))
					temp_service->stalk_on_warning=TRUE;
				else if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unknown"))
					temp_service->stalk_on_unknown=TRUE;
				else if(!strcmp(temp_ptr,"c") || !strcmp(temp_ptr,"critical"))
					temp_service->stalk_on_critical=TRUE;
				else{
#ifdef NSCORE
					printf("Error: Invalid stalking option '%s' in service definition.\n",temp_ptr);
#endif
					return ERROR;
				        }
			        }
			temp_service->have_stalking_options=TRUE;
		        }
		else if(!strcmp(variable,"process_perf_data")){
			temp_service->process_perf_data=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_process_perf_data=TRUE;
		        }
		else if(!strcmp(variable,"failure_prediction_enabled")){
			temp_service->failure_prediction_enabled=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_failure_prediction_enabled=TRUE;
		        }
		else if(!strcmp(variable,"retain_status_information")){
			temp_service->retain_status_information=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_retain_status_information=TRUE;
		        }
		else if(!strcmp(variable,"retain_nonstatus_information")){
			temp_service->retain_nonstatus_information=(atoi(value)>0)?TRUE:FALSE;
			temp_service->have_retain_nonstatus_information=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_service->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			printf("Error: Invalid service object directive '%s'.\n",variable);
#endif
			return ERROR;
		        }

		break;

	case XODTEMPLATE_HOSTDEPENDENCY:

		temp_hostdependency=(xodtemplate_hostdependency *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_hostdependency->template=(char *)malloc(strlen(value)+1);
			if(temp_hostdependency->template==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostdependency template.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostdependency->template,value);
		        }
		else if(!strcmp(variable,"name")){
			temp_hostdependency->name=(char *)malloc(strlen(value)+1);
			if(temp_hostdependency->name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostdependency name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostdependency->name,value);
		        }
		else if(!strcmp(variable,"host") || !strcmp(variable,"host_name") || !strcmp(variable,"master_host") || !strcmp(variable,"master_host_name")){
			temp_hostdependency->host_name=(char *)malloc(strlen(value)+1);
			if(temp_hostdependency->host_name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostdependency host.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostdependency->host_name,value);
		        }
		else if(!strcmp(variable,"dependent_host") || !strcmp(variable,"dependent_host_name")){
			temp_hostdependency->dependent_host_name=(char *)malloc(strlen(value)+1);
			if(temp_hostdependency->dependent_host_name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostdependency dependent host.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostdependency->dependent_host_name,value);
		        }
		else if(!strcmp(variable,"notification_failure_options") || !strcmp(variable,"notification_failure_criteria")){
			for(temp_ptr=strtok(value,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){
				if(!strcmp(temp_ptr,"o") || !strcmp(temp_ptr,"up"))
					temp_hostdependency->fail_notify_on_up=TRUE;
				else if(!strcmp(temp_ptr,"d") || !strcmp(temp_ptr,"down"))
					temp_hostdependency->fail_notify_on_down=TRUE;
				else if(!strcmp(temp_ptr,"u") || !strcmp(temp_ptr,"unreachable"))
					temp_hostdependency->fail_notify_on_unreachable=TRUE;
				else{
#ifdef NSCORE
					printf("Error: Invalid notification dependency option '%s' in hostdependency definition.\n",temp_ptr);
#endif
					return ERROR;
				        }
			        }
			temp_hostdependency->have_notification_dependency_options=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_hostdependency->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			printf("Error: Invalid hostdependency object directive '%s'.\n",variable);
#endif
			return ERROR;
		        }

		break;

	
	case XODTEMPLATE_HOSTESCALATION:

		temp_hostescalation=(xodtemplate_hostescalation *)xodtemplate_current_object;

		if(!strcmp(variable,"use")){
			temp_hostescalation->template=(char *)malloc(strlen(value)+1);
			if(temp_hostescalation->template==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostescalation template.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostescalation->template,value);
		        }
		else if(!strcmp(variable,"name")){
			temp_hostescalation->name=(char *)malloc(strlen(value)+1);
			if(temp_hostescalation->name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostescalation name.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostescalation->name,value);
		        }
		else if(!strcmp(variable,"hostgroup") || !strcmp(variable,"hostgroups") || !strcmp(variable,"hostgroup_name")){
			temp_hostescalation->hostgroup_name=(char *)malloc(strlen(value)+1);
			if(temp_hostescalation->hostgroup_name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostescalation hostgroup.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostescalation->hostgroup_name,value);
		        }
		else if(!strcmp(variable,"host") || !strcmp(variable,"host_name")){
			temp_hostescalation->host_name=(char *)malloc(strlen(value)+1);
			if(temp_hostescalation->host_name==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostescalation host.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostescalation->host_name,value);
		        }
		else if(!strcmp(variable,"contact_groups")){
			temp_hostescalation->contact_groups=(char *)malloc(strlen(value)+1);
			if(temp_hostescalation->contact_groups==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostescalation contact_groups.\n");
#endif
				return ERROR;
			        }
			strcpy(temp_hostescalation->contact_groups,value);
		        }
		else if(!strcmp(variable,"first_notification")){
			temp_hostescalation->first_notification=atoi(value);
			temp_hostescalation->have_first_notification=TRUE;
		        }
		else if(!strcmp(variable,"last_notification")){
			temp_hostescalation->last_notification=atoi(value);
			temp_hostescalation->have_last_notification=TRUE;
		        }
		else if(!strcmp(variable,"notification_interval")){
			temp_hostescalation->notification_interval=atoi(value);
			temp_hostescalation->have_notification_interval=TRUE;
		        }
		else if(!strcmp(variable,"register"))
			temp_hostescalation->register_object=(atoi(value)>0)?TRUE:FALSE;
		else{
#ifdef NSCORE
			printf("Error: Invalid hostescalation object directive '%s'.\n",variable);
#endif
			return ERROR;
		        }

		break;
	
	default:
		return ERROR;
		break;
	        }

#ifdef DEBUG0
	printf("xodtemplate_add_object_property() end\n");
#endif

	return result;
        }



/* completes an object definition */
int xodtemplate_end_object_definition(int options){
	int result=OK;

#ifdef DEBUG0
	printf("xodtemplate_end_object_definition() start\n");
#endif

	xodtemplate_current_object=NULL;
	xodtemplate_current_object_type=XODTEMPLATE_NONE;

#ifdef DEBUG0
	printf("xodtemplate_end_object_definition() end\n");
#endif

	return result;
        }





/******************************************************************/
/***************** OBJECT DUPLICATION FUNCTIONS *******************/
/******************************************************************/


/* duplicates object definitions */
int xodtemplate_duplicate_objects(void){
	int result=OK;
	xodtemplate_service *temp_service;
	xodtemplate_hostgroup *temp_hostgroup;
	xodtemplate_hostgroupescalation *temp_hostgroupescalation;
	xodtemplate_hostescalation *temp_hostescalation;
	xodtemplate_serviceescalation *temp_serviceescalation;
	char *host_names;
	char *hostgroup_names;
	char *temp_ptr;
	char *host_name_ptr;
	char *host_name;
	int first_item;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_objects() start\n");
#endif

	/****** RESOLVE ALL HOSTGROUP OBJECTS ******/
	/* HOSTGROUPS NEED TO BE RESOLVED BEFORE OBJECTS CAN BE DUPLICATED (BECAUSE SOME REFERENCE HOSTGROUPS) */
	for(temp_hostgroup=xodtemplate_hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){
		if(xodtemplate_resolve_hostgroup(temp_hostgroup)==ERROR)
			return ERROR;
	        }



	/****** DUPLICATE SERVICE DEFINITIONS WITH MULTIPLE HOST NAMES ******/
	for(temp_service=xodtemplate_service_list;temp_service!=NULL;temp_service=temp_service->next){

		/* skip services with NULL host names */
		if(temp_service->host_name==NULL)
			continue;

		/* this service definition should be assigned to multiple hosts... */
		if(strstr(temp_service->host_name,",")){
			
			/* allocate memory for host name list */
			host_names=(char *)malloc(strlen(temp_service->host_name)+1);
			if(host_names==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for host name list in service '%s' ('%s')\n",temp_service->name,temp_service->service_description);
#endif
				return ERROR;
			        }
			strcpy(host_names,temp_service->host_name);

			/* duplicate service entries */
			first_item=TRUE;
			for(temp_ptr=strtok(host_names,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){

				/* existing definition gets first host name (memory has already been allocated) */
				if(first_item==TRUE){
					strcpy(temp_service->host_name,temp_ptr);
					first_item=FALSE;
					continue;
				        }

				/* duplicate service definition */
				result=xodtemplate_duplicate_service(temp_service,temp_ptr);

				/* exit on error */
				if(result==ERROR){
					free(host_names);
					return ERROR;
				        }
			        }

			/* free memory we used for host name list */
			free(host_names);
		        }
	        }

	/****** DUPLICATE SERVICE DEFINITIONS WITH ONE OR MORE HOSTGROUP NAMES ******/
	for(temp_service=xodtemplate_service_list;temp_service!=NULL;temp_service=temp_service->next){

		/* skip services with NULL hostgroup names */
		if(temp_service->hostgroup_name==NULL)
			continue;
		
		/* allocate memory for hostgroup name list */
		hostgroup_names=(char *)malloc(strlen(temp_service->hostgroup_name)+1);
		if(hostgroup_names==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for hostgroup name list in service '%s' ('%s')\n",temp_service->name,temp_service->service_description);
#endif
			return ERROR;
		        }
		strcpy(hostgroup_names,temp_service->hostgroup_name);

		/* duplicate service entries */
		first_item=TRUE;
		for(temp_ptr=strtok(hostgroup_names,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){

			/* find the hostgroup */
			temp_hostgroup=xodtemplate_find_hostgroup(temp_ptr);
			if(temp_hostgroup==NULL){
#ifdef NSCORE
				printf("Error: Could not find hostgroup '%s' specified in service '%s' ('%s')\n",temp_ptr,temp_service->name,temp_service->service_description);
#endif
				return ERROR;
			        }

			host_names=(char *)malloc(strlen(temp_hostgroup->members)+1);
			if(host_names==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostgroup name list in service '%s' ('%s')\n",temp_service->name,temp_service->service_description);
#endif
				return ERROR;
		                }
			strcpy(host_names,temp_hostgroup->members);

			/* add a copy of the service for every host in the hostgroup */
			host_name_ptr=host_names;
			for(host_name=my_strsep(&host_name_ptr,", ");host_name!=NULL;host_name=my_strsep(&host_name_ptr,", ")){

				/* if this is the first duplication, see if we can use the existing entry */
				if(first_item==TRUE){

					if(temp_service->host_name==NULL){
						temp_service->host_name=(char *)malloc(strlen(host_name)+1);
						if(temp_service->host_name==NULL){
							free(hostgroup_names);
							return ERROR;
						        }
						strcpy(temp_service->host_name,host_name);
					        }
					else{
						result=xodtemplate_duplicate_service(temp_service,host_name);
						if(result==ERROR){
							free(hostgroup_names);
							return ERROR;
						        }
					        }

					first_item=FALSE;
					continue;
				        }

				/* duplicate service definition */
				result=xodtemplate_duplicate_service(temp_service,host_name);

				/* exit on error */
				if(result==ERROR){
					free(host_name);
					return ERROR;
			                }
			        }
		        }

		/* free memory we used for hostgroup name list */
		free(hostgroup_names);
	        }


	/****** DUPLICATE HOST ESCALATION DEFINITIONS WITH MULTIPLE HOST NAMES ******/
	for(temp_hostescalation=xodtemplate_hostescalation_list;temp_hostescalation!=NULL;temp_hostescalation=temp_hostescalation->next){

		/* skip hostescalations with NULL host names */
		if(temp_hostescalation->host_name==NULL)
			continue;

		/* this hostescalation definition should be assigned to multiple hosts... */
		if(strstr(temp_hostescalation->host_name,",")){
			
			/* allocate memory for host name list */
			host_names=(char *)malloc(strlen(temp_hostescalation->host_name)+1);
			if(host_names==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for host name list in hostescalation '%s' ('%s')\n",temp_hostescalation->name,temp_hostescalation->host_name);
#endif
				return ERROR;
			        }
			strcpy(host_names,temp_hostescalation->host_name);

			/* duplicate hostescalation entries */
			first_item=TRUE;
			for(temp_ptr=strtok(host_names,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){

				/* existing definition gets first host name (memory has already been allocated) */
				if(first_item==TRUE){
					strcpy(temp_hostescalation->host_name,temp_ptr);
					first_item=FALSE;
					continue;
				        }

				/* duplicate hostescalation definition */
				result=xodtemplate_duplicate_hostescalation(temp_hostescalation,temp_ptr);

				/* exit on error */
				if(result==ERROR){
					free(host_names);
					return ERROR;
				        }
			        }

			/* free memory we used for host name list */
			free(host_names);
		        }
	        }


	/****** DUPLICATE HOST ESCALATION DEFINITIONS WITH ONE OR MORE HOSTGROUP NAMES ******/
	for(temp_hostescalation=xodtemplate_hostescalation_list;temp_hostescalation!=NULL;temp_hostescalation=temp_hostescalation->next){

		/* skip hostescalations with NULL hostgroup names */
		if(temp_hostescalation->hostgroup_name==NULL)
			continue;
		
		/* allocate memory for hostgroup name list */
		hostgroup_names=(char *)malloc(strlen(temp_hostescalation->hostgroup_name)+1);
		if(hostgroup_names==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for hostgroup name list in hostescalation '%s'\n",temp_hostescalation->name);
#endif
			return ERROR;
		        }
		strcpy(hostgroup_names,temp_hostescalation->hostgroup_name);

		/* duplicate hostescalation entries */
		first_item=TRUE;
		for(temp_ptr=strtok(hostgroup_names,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){

			/* find the hostgroup */
			temp_hostgroup=xodtemplate_find_hostgroup(temp_ptr);
			if(temp_hostgroup==NULL){
#ifdef NSCORE
				printf("Error: Could not find hostgroup '%s' specified in hostescalation '%s'\n",temp_ptr,temp_hostescalation->name);
#endif
				return ERROR;
			        }

			host_names=(char *)malloc(strlen(temp_hostgroup->members)+1);
			if(host_names==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostgroup name list in hostescalation '%s'\n",temp_hostescalation->name);
#endif
				return ERROR;
		                }
			strcpy(host_names,temp_hostgroup->members);

			/* add a copy of the hostescalation for every host in the hostgroup */
			host_name_ptr=host_names;
			for(host_name=my_strsep(&host_name_ptr,", ");host_name!=NULL;host_name=my_strsep(&host_name_ptr,", ")){

				/* if this is the first duplication, see if we can use the existing entry */
				if(first_item==TRUE){

					if(temp_hostescalation->host_name==NULL){
						temp_hostescalation->host_name=(char *)malloc(strlen(host_name)+1);
						if(temp_hostescalation->host_name==NULL){
							free(hostgroup_names);
							return ERROR;
						        }
						strcpy(temp_hostescalation->host_name,host_name);
					        }
					else{
						result=xodtemplate_duplicate_hostescalation(temp_hostescalation,host_name);
						if(result==ERROR){
							free(hostgroup_names);
							return ERROR;
						        }
					        }

					first_item=FALSE;
					continue;
				        }

				/* duplicate hostescalation definition */
				result=xodtemplate_duplicate_hostescalation(temp_hostescalation,host_name);

				/* exit on error */
				if(result==ERROR){
					free(host_name);
					return ERROR;
			                }
			        }
		        }

		/* free memory we used for hostgroup name list */
		free(hostgroup_names);
	        }


	/****** DUPLICATE SERVICE ESCALATION DEFINITIONS WITH MULTIPLE HOST NAMES ******/
	for(temp_serviceescalation=xodtemplate_serviceescalation_list;temp_serviceescalation!=NULL;temp_serviceescalation=temp_serviceescalation->next){

		/* skip serviceescalations with NULL host names */
		if(temp_serviceescalation->host_name==NULL)
			continue;

		/* this serviceescalation definition should be assigned to multiple hosts... */
		if(strstr(temp_serviceescalation->host_name,",")){
			
			/* allocate memory for host name list */
			host_names=(char *)malloc(strlen(temp_serviceescalation->host_name)+1);
			if(host_names==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for host name list in serviceescalation '%s' ('%s')\n",temp_serviceescalation->name,temp_serviceescalation->host_name);
#endif
				return ERROR;
			        }
			strcpy(host_names,temp_serviceescalation->host_name);

			/* duplicate serviceescalation entries */
			first_item=TRUE;
			for(temp_ptr=strtok(host_names,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){

				/* existing definition gets first host name (memory has already been allocated) */
				if(first_item==TRUE){
					strcpy(temp_serviceescalation->host_name,temp_ptr);
					first_item=FALSE;
					continue;
				        }

				/* duplicate serviceescalation definition */
				result=xodtemplate_duplicate_serviceescalation(temp_serviceescalation,temp_ptr);

				/* exit on error */
				if(result==ERROR){
					free(host_names);
					return ERROR;
				        }
			        }

			/* free memory we used for host name list */
			free(host_names);
		        }
	        }


	/****** DUPLICATE HOSTGROUP ESCALATION DEFINITIONS WITH MULTIPLE HOSTGROUP NAMES ******/
	for(temp_hostgroupescalation=xodtemplate_hostgroupescalation_list;temp_hostgroupescalation!=NULL;temp_hostgroupescalation=temp_hostgroupescalation->next){

		/* skip hostgroup escalations with NULL hostgroup names */
		if(temp_hostgroupescalation->hostgroup_name==NULL)
			continue;

		/* this hostgroupescalation definition should be assigned to multiple hostgroups... */
		if(strstr(temp_hostgroupescalation->hostgroup_name,",")){
			
			/* allocate memory for hostgroup name list */
			hostgroup_names=(char *)malloc(strlen(temp_hostgroupescalation->hostgroup_name)+1);
			if(hostgroup_names==NULL){
#ifdef NSCORE
				printf("Error: Could not allocate memory for hostgroup name list in hostgroupescalation '%s'\n",temp_hostgroupescalation->name);
#endif
				return ERROR;
			        }
			strcpy(hostgroup_names,temp_hostgroupescalation->hostgroup_name);

			/* duplicate hostgroupescalation entries */
			first_item=TRUE;
			for(temp_ptr=strtok(hostgroup_names,", ");temp_ptr;temp_ptr=strtok(NULL,", ")){

				/* existing definition gets first hostgroup name (memory has already been allocated) */
				if(first_item==TRUE){
					strcpy(temp_hostgroupescalation->hostgroup_name,temp_ptr);
					first_item=FALSE;
					continue;
				        }

				/* duplicate hostgroupescalation definition */
				result=xodtemplate_duplicate_hostgroupescalation(temp_hostgroupescalation,temp_ptr);

				/* exit on error */
				if(result==ERROR){
					free(hostgroup_names);
					return ERROR;
				        }
			        }

			/* free memory we used for host name list */
			free(hostgroup_names);
		        }
	        }


#ifdef DEBUG0
	printf("xodtemplate_duplicate_objects() end\n");
#endif

	return OK;
        }



/* duplicates a service definition (with a new host name) */
int xodtemplate_duplicate_service(xodtemplate_service *temp_service, char *host_name){
	xodtemplate_service *new_service;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_service() start\n");
#endif

	/* allocate memory for a new service definition */
	new_service=(xodtemplate_service *)malloc(sizeof(xodtemplate_service));
	if(new_service==NULL){
#ifdef NSCORE
		printf("Error: Could not allocate memory for new duplicate service definition of '%s' ('%s').\n",temp_service->name,temp_service->service_description);
#endif
		return ERROR;
	        }

	/* defaults */
	new_service->template=NULL;
	new_service->name=NULL;
	new_service->hostgroup_name=NULL;
	new_service->host_name=NULL;
	new_service->service_description=NULL;
	new_service->check_command=NULL;
	new_service->check_period=NULL;
	new_service->event_handler=NULL;
	new_service->notification_period=NULL;
	new_service->contact_groups=NULL;
	new_service->failure_prediction_options=NULL;

	/* make sure hostgroup member in new service definition is NULL */
	new_service->hostgroup_name=NULL;

	new_service->max_check_attempts=temp_service->max_check_attempts;
	new_service->have_max_check_attempts=temp_service->have_max_check_attempts;
	new_service->normal_check_interval=temp_service->normal_check_interval;
	new_service->have_normal_check_interval=temp_service->have_normal_check_interval;
	new_service->retry_check_interval=temp_service->retry_check_interval;
	new_service->have_retry_check_interval=temp_service->have_retry_check_interval;
	new_service->active_checks_enabled=temp_service->active_checks_enabled;
	new_service->have_active_checks_enabled=temp_service->have_active_checks_enabled;
	new_service->passive_checks_enabled=temp_service->passive_checks_enabled;
	new_service->have_passive_checks_enabled=temp_service->have_passive_checks_enabled;
	new_service->parallelize_check=temp_service->parallelize_check;
	new_service->have_parallelize_check=temp_service->have_parallelize_check;
	new_service->is_volatile=temp_service->is_volatile;
	new_service->have_is_volatile=temp_service->have_is_volatile;
	new_service->obsess_over_service=temp_service->obsess_over_service;
	new_service->have_obsess_over_service=temp_service->have_obsess_over_service;
	new_service->event_handler_enabled=temp_service->event_handler_enabled;
	new_service->have_event_handler_enabled=temp_service->have_event_handler_enabled;
	new_service->check_freshness=temp_service->check_freshness;
	new_service->have_check_freshness=temp_service->have_check_freshness;
	new_service->freshness_threshold=temp_service->freshness_threshold;
	new_service->have_freshness_threshold=temp_service->have_freshness_threshold;
	new_service->flap_detection_enabled=temp_service->flap_detection_enabled;
	new_service->have_flap_detection_enabled=temp_service->have_flap_detection_enabled;
	new_service->low_flap_threshold=temp_service->low_flap_threshold;
	new_service->have_low_flap_threshold=temp_service->have_low_flap_threshold;
	new_service->high_flap_threshold=temp_service->high_flap_threshold;
	new_service->have_high_flap_threshold=temp_service->have_high_flap_threshold;
	new_service->notify_on_unknown=temp_service->notify_on_unknown;
	new_service->notify_on_warning=temp_service->notify_on_warning;
	new_service->notify_on_critical=temp_service->notify_on_critical;
	new_service->notify_on_recovery=temp_service->notify_on_recovery;
	new_service->have_notification_options=temp_service->have_notification_options;
	new_service->notifications_enabled=temp_service->notifications_enabled;
	new_service->have_notifications_enabled=temp_service->have_notifications_enabled;
	new_service->notification_interval=temp_service->notification_interval;
	new_service->have_notification_interval=temp_service->have_notification_interval;
	new_service->stalk_on_ok=temp_service->stalk_on_ok;
	new_service->stalk_on_unknown=temp_service->stalk_on_unknown;
	new_service->stalk_on_warning=temp_service->stalk_on_warning;
	new_service->stalk_on_critical=temp_service->stalk_on_critical;
	new_service->have_stalking_options=temp_service->have_stalking_options;
	new_service->process_perf_data=temp_service->process_perf_data;
	new_service->have_process_perf_data=temp_service->have_process_perf_data;
	new_service->failure_prediction_enabled=temp_service->failure_prediction_enabled;
	new_service->have_failure_prediction_enabled=temp_service->have_failure_prediction_enabled;
	new_service->retain_status_information=temp_service->retain_status_information;
	new_service->have_retain_status_information=temp_service->have_retain_status_information;
	new_service->retain_nonstatus_information=temp_service->retain_nonstatus_information;
	new_service->have_retain_nonstatus_information=temp_service->have_retain_nonstatus_information;

	new_service->has_been_resolved=temp_service->has_been_resolved;
	new_service->register_object=temp_service->register_object;

	/* allocate memory for and copy string members of service definition (host name provided, DO NOT duplicate hostgroup member!)*/
	if(temp_service->host_name!=NULL){
		new_service->host_name=(char *)malloc(strlen(host_name)+1);
		if(new_service->host_name==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s' ('%s').\n",temp_service->name,temp_service->service_description);
#endif
			free(new_service);
			return ERROR;
		        }
		strcpy(new_service->host_name,host_name);
	        } 
	if(temp_service->template!=NULL){
		new_service->template=(char *)malloc(strlen(temp_service->template)+1);
		if(new_service->template==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s' ('%s').\n",temp_service->name,temp_service->service_description);
#endif
			free(new_service->host_name);
			free(new_service);
			return ERROR;
		        }
		strcpy(new_service->template,temp_service->template);
	        } 
	if(temp_service->name!=NULL){
		new_service->name=(char *)malloc(strlen(temp_service->name)+1);
		if(new_service->name==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s' ('%s').\n",temp_service->name,temp_service->service_description);
#endif
			free(new_service->host_name);
			free(new_service->template);
			free(new_service);
			return ERROR;
		        }
		strcpy(new_service->name,temp_service->name);
	        } 
	if(temp_service->service_description!=NULL){
		new_service->service_description=(char *)malloc(strlen(temp_service->service_description)+1);
		if(new_service->service_description==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s' ('%s').\n",temp_service->name,temp_service->service_description);
#endif
			free(new_service->host_name);
			free(new_service->template);
			free(new_service->name);
			free(new_service);
			return ERROR;
		        }
		strcpy(new_service->service_description,temp_service->service_description);
	        } 
	if(temp_service->check_command!=NULL){
		new_service->check_command=(char *)malloc(strlen(temp_service->check_command)+1);
		if(new_service->check_command==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s' ('%s').\n",temp_service->name,temp_service->service_description);
#endif
			free(new_service->host_name);
			free(new_service->template);
			free(new_service->name);
			free(new_service->service_description);
			free(new_service);
			return ERROR;
		        }
		strcpy(new_service->check_command,temp_service->check_command);
	        } 
	if(temp_service->check_period!=NULL){
		new_service->check_period=(char *)malloc(strlen(temp_service->check_period)+1);
		if(new_service->check_period==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s' ('%s').\n",temp_service->name,temp_service->service_description);
#endif
			free(new_service->host_name);
			free(new_service->template);
			free(new_service->name);
			free(new_service->service_description);
			free(new_service->check_command);
			free(new_service);
			return ERROR;
		        }
		strcpy(new_service->check_period,temp_service->check_period);
	        } 
	if(temp_service->event_handler!=NULL){
		new_service->event_handler=(char *)malloc(strlen(temp_service->event_handler)+1);
		if(new_service->event_handler==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s' ('%s').\n",temp_service->name,temp_service->service_description);
#endif
			free(new_service->host_name);
			free(new_service->template);
			free(new_service->name);
			free(new_service->service_description);
			free(new_service->check_command);
			free(new_service->check_period);
			free(new_service);
			return ERROR;
		        }
		strcpy(new_service->event_handler,temp_service->event_handler);
	        } 
	if(temp_service->notification_period!=NULL){
		new_service->notification_period=(char *)malloc(strlen(temp_service->notification_period)+1);
		if(new_service->notification_period==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s' ('%s').\n",temp_service->name,temp_service->service_description);
#endif
			free(new_service->host_name);
			free(new_service->template);
			free(new_service->name);
			free(new_service->service_description);
			free(new_service->check_command);
			free(new_service->check_period);
			free(new_service->event_handler);
			free(new_service);
			return ERROR;
		        }
		strcpy(new_service->notification_period,temp_service->notification_period);
	        } 
	if(temp_service->contact_groups!=NULL){
		new_service->contact_groups=(char *)malloc(strlen(temp_service->contact_groups)+1);
		if(new_service->contact_groups==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s' ('%s').\n",temp_service->name,temp_service->service_description);
#endif
			free(new_service->host_name);
			free(new_service->template);
			free(new_service->name);
			free(new_service->service_description);
			free(new_service->check_command);
			free(new_service->check_period);
			free(new_service->event_handler);
			free(new_service->notification_period);
			free(new_service);
			return ERROR;
		        }
		strcpy(new_service->contact_groups,temp_service->contact_groups);
	        } 
	if(temp_service->failure_prediction_options!=NULL){
		new_service->failure_prediction_options=(char *)malloc(strlen(temp_service->failure_prediction_options)+1);
		if(new_service->failure_prediction_options==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s' ('%s').\n",temp_service->name,temp_service->service_description);
#endif
			free(new_service->host_name);
			free(new_service->template);
			free(new_service->name);
			free(new_service->service_description);
			free(new_service->check_command);
			free(new_service->check_period);
			free(new_service->event_handler);
			free(new_service->notification_period);
			free(new_service->contact_groups);
			free(new_service);
			return ERROR;
		        }
		strcpy(new_service->failure_prediction_options,temp_service->failure_prediction_options);
	        } 

	/* add new service to head of list in memory */
	new_service->next=xodtemplate_service_list;
	xodtemplate_service_list=new_service;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_service() end\n");
#endif

	return OK;
        }




/* duplicates a host escalation definition (with a new host name) */
int xodtemplate_duplicate_hostescalation(xodtemplate_hostescalation *temp_hostescalation, char *host_name){
	xodtemplate_hostescalation *new_hostescalation;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_hostescalation() start\n");
#endif

	/* allocate memory for a new host escalation definition */
	new_hostescalation=(xodtemplate_hostescalation *)malloc(sizeof(xodtemplate_hostescalation));
	if(new_hostescalation==NULL){
#ifdef NSCORE
		printf("Error: Could not allocate memory for new duplicate host escalation definition of '%s'.\n",temp_hostescalation->host_name);
#endif
		return ERROR;
	        }

	/* defaults */
	new_hostescalation->template=NULL;
	new_hostescalation->name=NULL;
	new_hostescalation->host_name=NULL;
	new_hostescalation->contact_groups=NULL;

	new_hostescalation->has_been_resolved=temp_hostescalation->has_been_resolved;
	new_hostescalation->register_object=temp_hostescalation->register_object;

	new_hostescalation->first_notification=temp_hostescalation->first_notification;
	new_hostescalation->last_notification=temp_hostescalation->last_notification;
	new_hostescalation->have_first_notification=temp_hostescalation->have_first_notification;
	new_hostescalation->have_last_notification=temp_hostescalation->have_last_notification;
	new_hostescalation->notification_interval=temp_hostescalation->notification_interval;
	new_hostescalation->have_notification_interval=temp_hostescalation->have_notification_interval;
	

	/* allocate memory for and copy string members of hostescalation definition */
	if(temp_hostescalation->host_name!=NULL){
		new_hostescalation->host_name=(char *)malloc(strlen(host_name)+1);
		if(new_hostescalation->host_name==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s'.\n",temp_hostescalation->host_name);
#endif
			free(new_hostescalation);
			return ERROR;
		        }
		strcpy(new_hostescalation->host_name,host_name);
	        } 
	if(temp_hostescalation->template!=NULL){
		new_hostescalation->template=(char *)malloc(strlen(temp_hostescalation->template)+1);
		if(new_hostescalation->template==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s'.\n",temp_hostescalation->host_name);
#endif
			free(new_hostescalation->host_name);
			free(new_hostescalation);
			return ERROR;
		        }
		strcpy(new_hostescalation->template,temp_hostescalation->template);
	        } 
	if(temp_hostescalation->name!=NULL){
		new_hostescalation->name=(char *)malloc(strlen(temp_hostescalation->name)+1);
		if(new_hostescalation->name==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s'.\n",temp_hostescalation->host_name);
#endif
			free(new_hostescalation->host_name);
			free(new_hostescalation->template);
			free(new_hostescalation);
			return ERROR;
		        }
		strcpy(new_hostescalation->name,temp_hostescalation->name);
	        } 
	if(temp_hostescalation->contact_groups!=NULL){
		new_hostescalation->contact_groups=(char *)malloc(strlen(temp_hostescalation->contact_groups)+1);
		if(new_hostescalation->contact_groups==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s'.\n",temp_hostescalation->host_name);
#endif
			free(new_hostescalation->host_name);
			free(new_hostescalation->template);
			free(new_hostescalation->name);
			free(new_hostescalation);
			return ERROR;
		        }
		strcpy(new_hostescalation->contact_groups,temp_hostescalation->contact_groups);
	        } 

	/* add new hostescalation to head of list in memory */
	new_hostescalation->next=xodtemplate_hostescalation_list;
	xodtemplate_hostescalation_list=new_hostescalation;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_hostescalation() end\n");
#endif

	return OK;
        }




/* duplicates a hostgroup escalation definition (with a new hostgroup name) */
int xodtemplate_duplicate_hostgroupescalation(xodtemplate_hostgroupescalation *temp_hostgroupescalation, char *hostgroup_name){
	xodtemplate_hostgroupescalation *new_hostgroupescalation;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_hostgroupescalation() start\n");
#endif

	/* allocate memory for a new host escalation definition */
	new_hostgroupescalation=(xodtemplate_hostgroupescalation *)malloc(sizeof(xodtemplate_hostgroupescalation));
	if(new_hostgroupescalation==NULL){
#ifdef NSCORE
		printf("Error: Could not allocate memory for new duplicate host escalation definition of '%s'.\n",temp_hostgroupescalation->hostgroup_name);
#endif
		return ERROR;
	        }

	/* defaults */
	new_hostgroupescalation->template=NULL;
	new_hostgroupescalation->name=NULL;
	new_hostgroupescalation->hostgroup_name=NULL;
	new_hostgroupescalation->contact_groups=NULL;

	new_hostgroupescalation->has_been_resolved=temp_hostgroupescalation->has_been_resolved;
	new_hostgroupescalation->register_object=temp_hostgroupescalation->register_object;

	new_hostgroupescalation->first_notification=temp_hostgroupescalation->first_notification;
	new_hostgroupescalation->last_notification=temp_hostgroupescalation->last_notification;
	new_hostgroupescalation->have_first_notification=temp_hostgroupescalation->have_first_notification;
	new_hostgroupescalation->have_last_notification=temp_hostgroupescalation->have_last_notification;
	new_hostgroupescalation->notification_interval=temp_hostgroupescalation->notification_interval;
	new_hostgroupescalation->have_notification_interval=temp_hostgroupescalation->have_notification_interval;
	

	/* allocate memory for and copy string members of hostgroupescalation definition */
	if(temp_hostgroupescalation->hostgroup_name!=NULL){
		new_hostgroupescalation->hostgroup_name=(char *)malloc(strlen(hostgroup_name)+1);
		if(new_hostgroupescalation->hostgroup_name==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s'.\n",temp_hostgroupescalation->hostgroup_name);
#endif
			free(new_hostgroupescalation);
			return ERROR;
		        }
		strcpy(new_hostgroupescalation->hostgroup_name,hostgroup_name);
	        } 
	if(temp_hostgroupescalation->template!=NULL){
		new_hostgroupescalation->template=(char *)malloc(strlen(temp_hostgroupescalation->template)+1);
		if(new_hostgroupescalation->template==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s'.\n",temp_hostgroupescalation->hostgroup_name);
#endif
			free(new_hostgroupescalation->hostgroup_name);
			free(new_hostgroupescalation);
			return ERROR;
		        }
		strcpy(new_hostgroupescalation->template,temp_hostgroupescalation->template);
	        } 
	if(temp_hostgroupescalation->name!=NULL){
		new_hostgroupescalation->name=(char *)malloc(strlen(temp_hostgroupescalation->name)+1);
		if(new_hostgroupescalation->name==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s'.\n",temp_hostgroupescalation->hostgroup_name);
#endif
			free(new_hostgroupescalation->hostgroup_name);
			free(new_hostgroupescalation->template);
			free(new_hostgroupescalation);
			return ERROR;
		        }
		strcpy(new_hostgroupescalation->name,temp_hostgroupescalation->name);
	        } 
	if(temp_hostgroupescalation->contact_groups!=NULL){
		new_hostgroupescalation->contact_groups=(char *)malloc(strlen(temp_hostgroupescalation->contact_groups)+1);
		if(new_hostgroupescalation->contact_groups==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s'.\n",temp_hostgroupescalation->hostgroup_name);
#endif
			free(new_hostgroupescalation->hostgroup_name);
			free(new_hostgroupescalation->template);
			free(new_hostgroupescalation->name);
			free(new_hostgroupescalation);
			return ERROR;
		        }
		strcpy(new_hostgroupescalation->contact_groups,temp_hostgroupescalation->contact_groups);
	        } 

	/* add new hostgroupescalation to head of list in memory */
	new_hostgroupescalation->next=xodtemplate_hostgroupescalation_list;
	xodtemplate_hostgroupescalation_list=new_hostgroupescalation;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_hostgroupescalation() end\n");
#endif

	return OK;
        }



/* duplicates a service escalation definition (with a new host name) */
int xodtemplate_duplicate_serviceescalation(xodtemplate_serviceescalation *temp_serviceescalation, char *host_name){
	xodtemplate_serviceescalation *new_serviceescalation;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_serviceescalation() start\n");
#endif

	/* allocate memory for a new host escalation definition */
	new_serviceescalation=(xodtemplate_serviceescalation *)malloc(sizeof(xodtemplate_serviceescalation));
	if(new_serviceescalation==NULL){
#ifdef NSCORE
		printf("Error: Could not allocate memory for new duplicate service escalation definition of '%s' on '%s'.\n",temp_serviceescalation->service_description,temp_serviceescalation->host_name);
#endif
		return ERROR;
	        }

	/* defaults */
	new_serviceescalation->template=NULL;
	new_serviceescalation->name=NULL;
	new_serviceescalation->host_name=NULL;
	new_serviceescalation->service_description=NULL;
	new_serviceescalation->contact_groups=NULL;

	new_serviceescalation->has_been_resolved=temp_serviceescalation->has_been_resolved;
	new_serviceescalation->register_object=temp_serviceescalation->register_object;

	new_serviceescalation->first_notification=temp_serviceescalation->first_notification;
	new_serviceescalation->last_notification=temp_serviceescalation->last_notification;
	new_serviceescalation->have_first_notification=temp_serviceescalation->have_first_notification;
	new_serviceescalation->have_last_notification=temp_serviceescalation->have_last_notification;
	new_serviceescalation->notification_interval=temp_serviceescalation->notification_interval;
	new_serviceescalation->have_notification_interval=temp_serviceescalation->have_notification_interval;
	

	/* allocate memory for and copy string members of serviceescalation definition */
	if(temp_serviceescalation->host_name!=NULL){
		new_serviceescalation->host_name=(char *)malloc(strlen(host_name)+1);
		if(new_serviceescalation->host_name==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s'.\n",temp_serviceescalation->host_name);
#endif
			free(new_serviceescalation);
			return ERROR;
		        }
		strcpy(new_serviceescalation->host_name,host_name);
	        } 
	if(temp_serviceescalation->template!=NULL){
		new_serviceescalation->template=(char *)malloc(strlen(temp_serviceescalation->template)+1);
		if(new_serviceescalation->template==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s'.\n",temp_serviceescalation->host_name);
#endif
			free(new_serviceescalation->host_name);
			free(new_serviceescalation);
			return ERROR;
		        }
		strcpy(new_serviceescalation->template,temp_serviceescalation->template);
	        } 
	if(temp_serviceescalation->name!=NULL){
		new_serviceescalation->name=(char *)malloc(strlen(temp_serviceescalation->name)+1);
		if(new_serviceescalation->name==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s'.\n",temp_serviceescalation->host_name);
#endif
			free(new_serviceescalation->host_name);
			free(new_serviceescalation->template);
			free(new_serviceescalation);
			return ERROR;
		        }
		strcpy(new_serviceescalation->name,temp_serviceescalation->name);
	        } 
	if(temp_serviceescalation->service_description!=NULL){
		new_serviceescalation->service_description=(char *)malloc(strlen(temp_serviceescalation->service_description)+1);
		if(new_serviceescalation->service_description==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s'.\n",temp_serviceescalation->host_name);
#endif
			free(new_serviceescalation->host_name);
			free(new_serviceescalation->template);
			free(new_serviceescalation->name);
			free(new_serviceescalation);
			return ERROR;
		        }
		strcpy(new_serviceescalation->service_description,temp_serviceescalation->service_description);
	        } 
	if(temp_serviceescalation->contact_groups!=NULL){
		new_serviceescalation->contact_groups=(char *)malloc(strlen(temp_serviceescalation->contact_groups)+1);
		if(new_serviceescalation->contact_groups==NULL){
#ifdef NSCORE
			printf("Error: Could not allocate memory for duplicate definition of '%s'.\n",temp_serviceescalation->host_name);
#endif
			free(new_serviceescalation->service_description);
			free(new_serviceescalation->host_name);
			free(new_serviceescalation->template);
			free(new_serviceescalation->name);
			free(new_serviceescalation);
			return ERROR;
		        }
		strcpy(new_serviceescalation->contact_groups,temp_serviceescalation->contact_groups);
	        } 

	/* add new serviceescalation to head of list in memory */
	new_serviceescalation->next=xodtemplate_serviceescalation_list;
	xodtemplate_serviceescalation_list=new_serviceescalation;

#ifdef DEBUG0
	printf("xodtemplate_duplicate_serviceescalation() end\n");
#endif

	return OK;
        }





/******************************************************************/
/***************** OBJECT RESOLUTION FUNCTIONS ********************/
/******************************************************************/

/* resolves object definitions */
int xodtemplate_resolve_objects(void){
	xodtemplate_timeperiod *temp_timeperiod;
	xodtemplate_command *temp_command;
	xodtemplate_contactgroup *temp_contactgroup;
	/*xodtemplate_hostgroup *temp_hostgroup;*/
	xodtemplate_servicedependency *temp_servicedependency;
	xodtemplate_serviceescalation *temp_serviceescalation;
	xodtemplate_hostgroupescalation *temp_hostgroupescalation;
	xodtemplate_contact *temp_contact;
	xodtemplate_host *temp_host;
	xodtemplate_service *temp_service;
	xodtemplate_hostdependency *temp_hostdependency;
	xodtemplate_hostescalation *temp_hostescalation;

#ifdef DEBUG0
	printf("xodtemplate_resolve_objects() start\n");
#endif

	/* resolve all timeperiod objects */
	for(temp_timeperiod=xodtemplate_timeperiod_list;temp_timeperiod!=NULL;temp_timeperiod=temp_timeperiod->next){
		if(xodtemplate_resolve_timeperiod(temp_timeperiod)==ERROR)
			return ERROR;
	        }

	/* resolve all command objects */
	for(temp_command=xodtemplate_command_list;temp_command!=NULL;temp_command=temp_command->next){
		if(xodtemplate_resolve_command(temp_command)==ERROR)
			return ERROR;
	        }

	/* resolve all contactgroup objects */
	for(temp_contactgroup=xodtemplate_contactgroup_list;temp_contactgroup!=NULL;temp_contactgroup=temp_contactgroup->next){
		if(xodtemplate_resolve_contactgroup(temp_contactgroup)==ERROR)
			return ERROR;
	        }

	/* HOSTGROUPS DON'T NEED TO BE RESOLVED HERE BECAUSE THEY WERE ALREADY RESOLVED IN duplicate_objects() */
	/* resolve all hostgroup objects */
	/*
	for(temp_hostgroup=xodtemplate_hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){
		if(xodtemplate_resolve_hostgroup(temp_hostgroup)==ERROR)
			return ERROR;
	        }
	*/

	/* resolve all servicedependency objects */
	for(temp_servicedependency=xodtemplate_servicedependency_list;temp_servicedependency!=NULL;temp_servicedependency=temp_servicedependency->next){
		if(xodtemplate_resolve_servicedependency(temp_servicedependency)==ERROR)
			return ERROR;
	        }

	/* resolve all serviceescalation objects */
	for(temp_serviceescalation=xodtemplate_serviceescalation_list;temp_serviceescalation!=NULL;temp_serviceescalation=temp_serviceescalation->next){
		if(xodtemplate_resolve_serviceescalation(temp_serviceescalation)==ERROR)
			return ERROR;
	        }

	/* resolve all hostgroupescalation objects */
	for(temp_hostgroupescalation=xodtemplate_hostgroupescalation_list;temp_hostgroupescalation!=NULL;temp_hostgroupescalation=temp_hostgroupescalation->next){
		if(xodtemplate_resolve_hostgroupescalation(temp_hostgroupescalation)==ERROR)
			return ERROR;
	        }

	/* resolve all contact objects */
	for(temp_contact=xodtemplate_contact_list;temp_contact!=NULL;temp_contact=temp_contact->next){
		if(xodtemplate_resolve_contact(temp_contact)==ERROR)
			return ERROR;
	        }

	/* resolve all host objects */
	for(temp_host=xodtemplate_host_list;temp_host!=NULL;temp_host=temp_host->next){
		if(xodtemplate_resolve_host(temp_host)==ERROR)
			return ERROR;
	        }

	/* resolve all service objects */
	for(temp_service=xodtemplate_service_list;temp_service!=NULL;temp_service=temp_service->next){
		if(xodtemplate_resolve_service(temp_service)==ERROR)
			return ERROR;
	        }

	/* resolve all hostdependency objects */
	for(temp_hostdependency=xodtemplate_hostdependency_list;temp_hostdependency!=NULL;temp_hostdependency=temp_hostdependency->next){
		if(xodtemplate_resolve_hostdependency(temp_hostdependency)==ERROR)
			return ERROR;
	        }

	/* resolve all hostescalation objects */
	for(temp_hostescalation=xodtemplate_hostescalation_list;temp_hostescalation!=NULL;temp_hostescalation=temp_hostescalation->next){
		if(xodtemplate_resolve_hostescalation(temp_hostescalation)==ERROR)
			return ERROR;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_objects() end\n");
#endif

	return OK;
        }



/* resolves a timperiod object */
int xodtemplate_resolve_timeperiod(xodtemplate_timeperiod *this_timeperiod){
	xodtemplate_timeperiod *template_timeperiod;
	int x;

#ifdef DEBUG0
	printf("xodtemplate_resolve_timperiod() start\n");
#endif

	/* return if this timperiod has already been resolved */
	if(this_timeperiod->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_timeperiod->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_timeperiod->template==NULL)
		return OK;

	template_timeperiod=xodtemplate_find_timeperiod(this_timeperiod->template);
	if(template_timeperiod==NULL){
#ifdef NSCORE
		printf("Error: Template '%s' specified in timeperiod '%s' definition not found!\n",this_timeperiod->template,(this_timeperiod->name==NULL)?"NULL":this_timeperiod->name);
#endif
		return ERROR;
	        }

	/* resolve the template timeperiod... */
	xodtemplate_resolve_timeperiod(template_timeperiod);

	/* apply missing properties from template timeperiod... */
	if(this_timeperiod->timeperiod_name==NULL && template_timeperiod->timeperiod_name!=NULL){
		this_timeperiod->timeperiod_name=(char *)malloc(strlen(template_timeperiod->timeperiod_name)+1);
		if(this_timeperiod->timeperiod_name!=NULL)
			strcpy(this_timeperiod->timeperiod_name,template_timeperiod->timeperiod_name);
	        }
	if(this_timeperiod->alias==NULL && template_timeperiod->alias!=NULL){
		this_timeperiod->alias=(char *)malloc(strlen(template_timeperiod->alias)+1);
		if(this_timeperiod->alias!=NULL)
			strcpy(this_timeperiod->alias,template_timeperiod->alias);
	        }
	for(x=0;x<7;x++){
		if(this_timeperiod->timeranges[x]==NULL && template_timeperiod->timeranges[x]!=NULL){
			this_timeperiod->timeranges[x]=(char *)malloc(strlen(template_timeperiod->timeranges[x])+1);
			if(this_timeperiod->timeranges[x]!=NULL)
				strcpy(this_timeperiod->timeranges[x],template_timeperiod->timeranges[x]);
		        }
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_timeperiod() end\n");
#endif

	return OK;
        }




/* resolves a command object */
int xodtemplate_resolve_command(xodtemplate_command *this_command){
	xodtemplate_command *template_command;

#ifdef DEBUG0
	printf("xodtemplate_resolve_command() start\n");
#endif

	/* return if this command has already been resolved */
	if(this_command->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_command->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_command->template==NULL)
		return OK;

	template_command=xodtemplate_find_command(this_command->template);
	if(template_command==NULL){
#ifdef NSCORE
		printf("Error: Template '%s' specified in command '%s' definition not found!\n",this_command->template,(this_command->name==NULL)?"NULL":this_command->name);
#endif
		return ERROR;
	        }

	/* resolve the template command... */
	xodtemplate_resolve_command(template_command);

	/* apply missing properties from template command... */
	if(this_command->command_name==NULL && template_command->command_name!=NULL){
		this_command->command_name=(char *)malloc(strlen(template_command->command_name)+1);
		if(this_command->command_name!=NULL)
			strcpy(this_command->command_name,template_command->command_name);
	        }
	if(this_command->command_line==NULL && template_command->command_line!=NULL){
		this_command->command_line=(char *)malloc(strlen(template_command->command_line)+1);
		if(this_command->command_line!=NULL)
			strcpy(this_command->command_line,template_command->command_line);
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_command() end\n");
#endif

	return OK;
        }




/* resolves a contactgroup object */
int xodtemplate_resolve_contactgroup(xodtemplate_contactgroup *this_contactgroup){
	xodtemplate_contactgroup *template_contactgroup;

#ifdef DEBUG0
	printf("xodtemplate_resolve_contactgroup() start\n");
#endif

	/* return if this contactgroup has already been resolved */
	if(this_contactgroup->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_contactgroup->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_contactgroup->template==NULL)
		return OK;

	template_contactgroup=xodtemplate_find_contactgroup(this_contactgroup->template);
	if(template_contactgroup==NULL){
#ifdef NSCORE
		printf("Error: Template '%s' specified in contactgroup '%s' definition not found!\n",this_contactgroup->template,(this_contactgroup->name==NULL)?"NULL":this_contactgroup->name);
#endif
		return ERROR;
	        }

	/* resolve the template contactgroup... */
	xodtemplate_resolve_contactgroup(template_contactgroup);

	/* apply missing properties from template contactgroup... */
	if(this_contactgroup->contactgroup_name==NULL && template_contactgroup->contactgroup_name!=NULL){
		this_contactgroup->contactgroup_name=(char *)malloc(strlen(template_contactgroup->contactgroup_name)+1);
		if(this_contactgroup->contactgroup_name!=NULL)
			strcpy(this_contactgroup->contactgroup_name,template_contactgroup->contactgroup_name);
	        }
	if(this_contactgroup->alias==NULL && template_contactgroup->alias!=NULL){
		this_contactgroup->alias=(char *)malloc(strlen(template_contactgroup->alias)+1);
		if(this_contactgroup->alias!=NULL)
			strcpy(this_contactgroup->alias,template_contactgroup->alias);
	        }
	if(this_contactgroup->members==NULL && template_contactgroup->members!=NULL){
		this_contactgroup->members=(char *)malloc(strlen(template_contactgroup->members)+1);
		if(this_contactgroup->members!=NULL)
			strcpy(this_contactgroup->members,template_contactgroup->members);
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_contactgroup() end\n");
#endif

	return OK;
        }




/* resolves a hostgroup object */
int xodtemplate_resolve_hostgroup(xodtemplate_hostgroup *this_hostgroup){
	xodtemplate_hostgroup *template_hostgroup;

#ifdef DEBUG0
	printf("xodtemplate_resolve_hostgroup() start\n");
#endif

	/* return if this hostgroup has already been resolved */
	if(this_hostgroup->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_hostgroup->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_hostgroup->template==NULL)
		return OK;

	template_hostgroup=xodtemplate_find_hostgroup(this_hostgroup->template);
	if(template_hostgroup==NULL){
#ifdef NSCORE
		printf("Error: Template '%s' specified in hostgroup '%s' definition not found!\n",this_hostgroup->template,(this_hostgroup->name==NULL)?"NULL":this_hostgroup->name);
#endif
		return ERROR;
	        }

	/* resolve the template hostgroup... */
	xodtemplate_resolve_hostgroup(template_hostgroup);

	/* apply missing properties from template hostgroup... */
	if(this_hostgroup->hostgroup_name==NULL && template_hostgroup->hostgroup_name!=NULL){
		this_hostgroup->hostgroup_name=(char *)malloc(strlen(template_hostgroup->hostgroup_name)+1);
		if(this_hostgroup->hostgroup_name!=NULL)
			strcpy(this_hostgroup->hostgroup_name,template_hostgroup->hostgroup_name);
	        }
	if(this_hostgroup->alias==NULL && template_hostgroup->alias!=NULL){
		this_hostgroup->alias=(char *)malloc(strlen(template_hostgroup->alias)+1);
		if(this_hostgroup->alias!=NULL)
			strcpy(this_hostgroup->alias,template_hostgroup->alias);
	        }
	if(this_hostgroup->members==NULL && template_hostgroup->members!=NULL){
		this_hostgroup->members=(char *)malloc(strlen(template_hostgroup->members)+1);
		if(this_hostgroup->members!=NULL)
			strcpy(this_hostgroup->members,template_hostgroup->members);
	        }
	if(this_hostgroup->contact_groups==NULL && template_hostgroup->contact_groups!=NULL){
		this_hostgroup->contact_groups=(char *)malloc(strlen(template_hostgroup->contact_groups)+1);
		if(this_hostgroup->contact_groups!=NULL)
			strcpy(this_hostgroup->contact_groups,template_hostgroup->contact_groups);
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_hostgroup() end\n");
#endif

	return OK;
        }


/* resolves a servicedependency object */
int xodtemplate_resolve_servicedependency(xodtemplate_servicedependency *this_servicedependency){
	xodtemplate_servicedependency *template_servicedependency;

#ifdef DEBUG0
	printf("xodtemplate_resolve_servicedependency() start\n");
#endif

	/* return if this servicedependency has already been resolved */
	if(this_servicedependency->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_servicedependency->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_servicedependency->template==NULL)
		return OK;

	template_servicedependency=xodtemplate_find_servicedependency(this_servicedependency->template);
	if(template_servicedependency==NULL){
#ifdef NSCORE
		printf("Error: Template '%s' specified in servicedependency '%s' definition not found!\n",this_servicedependency->template,(this_servicedependency->name==NULL)?"NULL":this_servicedependency->name);
#endif
		return ERROR;
	        }

	/* resolve the template servicedependency... */
	xodtemplate_resolve_servicedependency(template_servicedependency);

	/* apply missing properties from template servicedependency... */
	if(this_servicedependency->host_name==NULL && template_servicedependency->host_name!=NULL){
		this_servicedependency->host_name=(char *)malloc(strlen(template_servicedependency->host_name)+1);
		if(this_servicedependency->host_name!=NULL)
			strcpy(this_servicedependency->host_name,template_servicedependency->host_name);
	        }
	if(this_servicedependency->service_description==NULL && template_servicedependency->service_description!=NULL){
		this_servicedependency->service_description=(char *)malloc(strlen(template_servicedependency->service_description)+1);
		if(this_servicedependency->service_description!=NULL)
			strcpy(this_servicedependency->service_description,template_servicedependency->service_description);
	        }
	if(this_servicedependency->dependent_host_name==NULL && template_servicedependency->dependent_host_name!=NULL){
		this_servicedependency->dependent_host_name=(char *)malloc(strlen(template_servicedependency->dependent_host_name)+1);
		if(this_servicedependency->dependent_host_name!=NULL)
			strcpy(this_servicedependency->dependent_host_name,template_servicedependency->dependent_host_name);
	        }
	if(this_servicedependency->dependent_service_description==NULL && template_servicedependency->dependent_service_description!=NULL){
		this_servicedependency->dependent_service_description=(char *)malloc(strlen(template_servicedependency->dependent_service_description)+1);
		if(this_servicedependency->dependent_service_description!=NULL)
			strcpy(this_servicedependency->dependent_service_description,template_servicedependency->dependent_service_description);
	        }
	if(this_servicedependency->have_execution_dependency_options==FALSE && template_servicedependency->have_execution_dependency_options==TRUE){
		this_servicedependency->fail_execute_on_ok=template_servicedependency->fail_execute_on_ok;
		this_servicedependency->fail_execute_on_unknown=template_servicedependency->fail_execute_on_unknown;
		this_servicedependency->fail_execute_on_warning=template_servicedependency->fail_execute_on_warning;
		this_servicedependency->fail_execute_on_critical=template_servicedependency->fail_execute_on_critical;
		this_servicedependency->have_execution_dependency_options=TRUE;
	        }
	if(this_servicedependency->have_notification_dependency_options==FALSE && template_servicedependency->have_notification_dependency_options==TRUE){
		this_servicedependency->fail_notify_on_ok=template_servicedependency->fail_notify_on_ok;
		this_servicedependency->fail_notify_on_unknown=template_servicedependency->fail_notify_on_unknown;
		this_servicedependency->fail_notify_on_warning=template_servicedependency->fail_notify_on_warning;
		this_servicedependency->fail_notify_on_critical=template_servicedependency->fail_notify_on_critical;
		this_servicedependency->have_notification_dependency_options=TRUE;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_servicedependency() end\n");
#endif

	return OK;
        }


/* resolves a serviceescalation object */
int xodtemplate_resolve_serviceescalation(xodtemplate_serviceescalation *this_serviceescalation){
	xodtemplate_serviceescalation *template_serviceescalation;

#ifdef DEBUG0
	printf("xodtemplate_resolve_serviceescalation() start\n");
#endif

	/* return if this serviceescalation has already been resolved */
	if(this_serviceescalation->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_serviceescalation->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_serviceescalation->template==NULL)
		return OK;

	template_serviceescalation=xodtemplate_find_serviceescalation(this_serviceescalation->template);
	if(template_serviceescalation==NULL){
#ifdef NSCORE
		printf("Error: Template '%s' specified in serviceescalation '%s' definition not found!\n",this_serviceescalation->template,(this_serviceescalation->name==NULL)?"NULL":this_serviceescalation->name);
#endif
		return ERROR;
	        }

	/* resolve the template serviceescalation... */
	xodtemplate_resolve_serviceescalation(template_serviceescalation);

	/* apply missing properties from template serviceescalation... */
	if(this_serviceescalation->host_name==NULL && template_serviceescalation->host_name!=NULL){
		this_serviceescalation->host_name=(char *)malloc(strlen(template_serviceescalation->host_name)+1);
		if(this_serviceescalation->host_name!=NULL)
			strcpy(this_serviceescalation->host_name,template_serviceescalation->host_name);
	        }
	if(this_serviceescalation->service_description==NULL && template_serviceescalation->service_description!=NULL){
		this_serviceescalation->service_description=(char *)malloc(strlen(template_serviceescalation->service_description)+1);
		if(this_serviceescalation->service_description!=NULL)
			strcpy(this_serviceescalation->service_description,template_serviceescalation->service_description);
	        }
	if(this_serviceescalation->contact_groups==NULL && template_serviceescalation->contact_groups!=NULL){
		this_serviceescalation->contact_groups=(char *)malloc(strlen(template_serviceescalation->contact_groups)+1);
		if(this_serviceescalation->contact_groups!=NULL)
			strcpy(this_serviceescalation->contact_groups,template_serviceescalation->contact_groups);
	        }
	if(this_serviceescalation->have_first_notification==FALSE && template_serviceescalation->have_first_notification==TRUE){
		this_serviceescalation->first_notification=template_serviceescalation->first_notification;
		this_serviceescalation->have_first_notification=TRUE;
	        }
	if(this_serviceescalation->have_last_notification==FALSE && template_serviceescalation->have_last_notification==TRUE){
		this_serviceescalation->last_notification=template_serviceescalation->last_notification;
		this_serviceescalation->have_last_notification=TRUE;
	        }
	if(this_serviceescalation->have_notification_interval==FALSE && template_serviceescalation->have_notification_interval==TRUE){
		this_serviceescalation->notification_interval=template_serviceescalation->notification_interval;
		this_serviceescalation->have_notification_interval=TRUE;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_servicedependency() end\n");
#endif

	return OK;
        }



/* resolves a hostgroupescalation object */
int xodtemplate_resolve_hostgroupescalation(xodtemplate_hostgroupescalation *this_hostgroupescalation){
	xodtemplate_hostgroupescalation *template_hostgroupescalation;

#ifdef DEBUG0
	printf("xodtemplate_resolve_hostgroupescalation() start\n");
#endif

	/* return if this hostgroupescalation has already been resolved */
	if(this_hostgroupescalation->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_hostgroupescalation->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_hostgroupescalation->template==NULL)
		return OK;

	template_hostgroupescalation=xodtemplate_find_hostgroupescalation(this_hostgroupescalation->template);
	if(template_hostgroupescalation==NULL){
#ifdef NSCORE
		printf("Error: Template '%s' specified in hostgroupescalation '%s' definition not found!\n",this_hostgroupescalation->template,(this_hostgroupescalation->name==NULL)?"NULL":this_hostgroupescalation->name);
#endif
		return ERROR;
	        }

	/* resolve the template hostgroupescalation... */
	xodtemplate_resolve_hostgroupescalation(template_hostgroupescalation);

	/* apply missing properties from template hostgroupescalation... */
	if(this_hostgroupescalation->hostgroup_name==NULL && template_hostgroupescalation->hostgroup_name!=NULL){
		this_hostgroupescalation->hostgroup_name=(char *)malloc(strlen(template_hostgroupescalation->hostgroup_name)+1);
		if(this_hostgroupescalation->hostgroup_name!=NULL)
			strcpy(this_hostgroupescalation->hostgroup_name,template_hostgroupescalation->hostgroup_name);
	        }
	if(this_hostgroupescalation->contact_groups==NULL && template_hostgroupescalation->contact_groups!=NULL){
		this_hostgroupescalation->contact_groups=(char *)malloc(strlen(template_hostgroupescalation->contact_groups)+1);
		if(this_hostgroupescalation->contact_groups!=NULL)
			strcpy(this_hostgroupescalation->contact_groups,template_hostgroupescalation->contact_groups);
	        }
	if(this_hostgroupescalation->have_first_notification==FALSE && template_hostgroupescalation->have_first_notification==TRUE){
		this_hostgroupescalation->first_notification=template_hostgroupescalation->first_notification;
		this_hostgroupescalation->have_first_notification=TRUE;
	        }
	if(this_hostgroupescalation->have_last_notification==FALSE && template_hostgroupescalation->have_last_notification==TRUE){
		this_hostgroupescalation->last_notification=template_hostgroupescalation->last_notification;
		this_hostgroupescalation->have_last_notification=TRUE;
	        }
	if(this_hostgroupescalation->have_notification_interval==FALSE && template_hostgroupescalation->have_notification_interval==TRUE){
		this_hostgroupescalation->notification_interval=template_hostgroupescalation->notification_interval;
		this_hostgroupescalation->have_notification_interval=TRUE;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_hostgroupescalation() end\n");
#endif

	return OK;
        }



/* resolves a contact object */
int xodtemplate_resolve_contact(xodtemplate_contact *this_contact){
	xodtemplate_contact *template_contact;

#ifdef DEBUG0
	printf("xodtemplate_resolve_contact() start\n");
#endif

	/* return if this contact has already been resolved */
	if(this_contact->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_contact->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_contact->template==NULL)
		return OK;

	template_contact=xodtemplate_find_contact(this_contact->template);
	if(template_contact==NULL){
#ifdef NSCORE
		printf("Error: Template '%s' specified in contact '%s' definition not found!\n",this_contact->template,(this_contact->name==NULL)?"NULL":this_contact->name);
#endif
		return ERROR;
	        }

	/* resolve the template contact... */
	xodtemplate_resolve_contact(template_contact);

	/* apply missing properties from template contact... */
	if(this_contact->contact_name==NULL && template_contact->contact_name!=NULL){
		this_contact->contact_name=(char *)malloc(strlen(template_contact->contact_name)+1);
		if(this_contact->contact_name!=NULL)
			strcpy(this_contact->contact_name,template_contact->contact_name);
	        }
	if(this_contact->alias==NULL && template_contact->alias!=NULL){
		this_contact->alias=(char *)malloc(strlen(template_contact->alias)+1);
		if(this_contact->alias!=NULL)
			strcpy(this_contact->alias,template_contact->alias);
	        }
	if(this_contact->email==NULL && template_contact->email!=NULL){
		this_contact->email=(char *)malloc(strlen(template_contact->email)+1);
		if(this_contact->email!=NULL)
			strcpy(this_contact->email,template_contact->email);
	        }
	if(this_contact->pager==NULL && template_contact->pager!=NULL){
		this_contact->pager=(char *)malloc(strlen(template_contact->pager)+1);
		if(this_contact->pager!=NULL)
			strcpy(this_contact->pager,template_contact->pager);
	        }
	if(this_contact->host_notification_period==NULL && template_contact->host_notification_period!=NULL){
		this_contact->host_notification_period=(char *)malloc(strlen(template_contact->host_notification_period)+1);
		if(this_contact->host_notification_period!=NULL)
			strcpy(this_contact->host_notification_period,template_contact->host_notification_period);
	        }
	if(this_contact->service_notification_period==NULL && template_contact->service_notification_period!=NULL){
		this_contact->service_notification_period=(char *)malloc(strlen(template_contact->service_notification_period)+1);
		if(this_contact->service_notification_period!=NULL)
			strcpy(this_contact->service_notification_period,template_contact->service_notification_period);
	        }
	if(this_contact->host_notification_commands==NULL && template_contact->host_notification_commands!=NULL){
		this_contact->host_notification_commands=(char *)malloc(strlen(template_contact->host_notification_commands)+1);
		if(this_contact->host_notification_commands!=NULL)
			strcpy(this_contact->host_notification_commands,template_contact->host_notification_commands);
	        }
	if(this_contact->service_notification_commands==NULL && template_contact->service_notification_commands!=NULL){
		this_contact->service_notification_commands=(char *)malloc(strlen(template_contact->service_notification_commands)+1);
		if(this_contact->service_notification_commands!=NULL)
			strcpy(this_contact->service_notification_commands,template_contact->service_notification_commands);
	        }
	if(this_contact->have_host_notification_options==FALSE && template_contact->have_host_notification_options==TRUE){
		this_contact->notify_on_host_down=template_contact->notify_on_host_down;
		this_contact->notify_on_host_unreachable=template_contact->notify_on_host_unreachable;
		this_contact->notify_on_host_recovery=template_contact->notify_on_host_recovery;
		this_contact->have_host_notification_options=TRUE;
	        }
	if(this_contact->have_service_notification_options==FALSE && template_contact->have_service_notification_options==TRUE){
		this_contact->notify_on_service_unknown=template_contact->notify_on_service_unknown;
		this_contact->notify_on_service_warning=template_contact->notify_on_service_warning;
		this_contact->notify_on_service_critical=template_contact->notify_on_service_critical;
		this_contact->notify_on_service_recovery=template_contact->notify_on_service_recovery;
		this_contact->have_service_notification_options=TRUE;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_contact() end\n");
#endif

	return OK;
        }



/* resolves a host object */
int xodtemplate_resolve_host(xodtemplate_host *this_host){
	xodtemplate_host *template_host;

#ifdef DEBUG0
	printf("xodtemplate_resolve_host() start\n");
#endif

	/* return if this host has already been resolved */
	if(this_host->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_host->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_host->template==NULL)
		return OK;

	template_host=xodtemplate_find_host(this_host->template);
	if(template_host==NULL){
#ifdef NSCORE
		printf("Error: Template '%s' specified in host '%s' definition not found!\n",this_host->template,(this_host->name==NULL)?"NULL":this_host->name);
#endif
		return ERROR;
	        }

	/* resolve the template host... */
	xodtemplate_resolve_host(template_host);

	/* apply missing properties from template host... */
	if(this_host->host_name==NULL && template_host->host_name!=NULL){
		this_host->host_name=(char *)malloc(strlen(template_host->host_name)+1);
		if(this_host->host_name!=NULL)
			strcpy(this_host->host_name,template_host->host_name);
	        }
	if(this_host->alias==NULL && template_host->alias!=NULL){
		this_host->alias=(char *)malloc(strlen(template_host->alias)+1);
		if(this_host->alias!=NULL)
			strcpy(this_host->alias,template_host->alias);
	        }
	if(this_host->address==NULL && template_host->address!=NULL){
		this_host->address=(char *)malloc(strlen(template_host->address)+1);
		if(this_host->address!=NULL)
			strcpy(this_host->address,template_host->address);
	        }
	if(this_host->parents==NULL && template_host->parents!=NULL){
		this_host->parents=(char *)malloc(strlen(template_host->parents)+1);
		if(this_host->parents!=NULL)
			strcpy(this_host->parents,template_host->parents);
	        }
	if(this_host->check_command==NULL && template_host->check_command!=NULL){
		this_host->check_command=(char *)malloc(strlen(template_host->check_command)+1);
		if(this_host->check_command!=NULL)
			strcpy(this_host->check_command,template_host->check_command);
	        }
	if(this_host->event_handler==NULL && template_host->event_handler!=NULL){
		this_host->event_handler=(char *)malloc(strlen(template_host->event_handler)+1);
		if(this_host->event_handler!=NULL)
			strcpy(this_host->event_handler,template_host->event_handler);
	        }
	if(this_host->notification_period==NULL && template_host->notification_period!=NULL){
		this_host->notification_period=(char *)malloc(strlen(template_host->notification_period)+1);
		if(this_host->notification_period!=NULL)
			strcpy(this_host->notification_period,template_host->notification_period);
	        }
	if(this_host->failure_prediction_options==NULL && template_host->failure_prediction_options!=NULL){
		this_host->failure_prediction_options=(char *)malloc(strlen(template_host->failure_prediction_options)+1);
		if(this_host->failure_prediction_options!=NULL)
			strcpy(this_host->failure_prediction_options,template_host->failure_prediction_options);
	        }
	if(this_host->have_max_check_attempts==FALSE && template_host->have_max_check_attempts==TRUE){
		this_host->max_check_attempts=template_host->max_check_attempts;
		this_host->have_max_check_attempts=TRUE;
	        }
	if(this_host->have_checks_enabled==FALSE && template_host->have_checks_enabled==TRUE){
		this_host->checks_enabled=template_host->checks_enabled;
		this_host->have_checks_enabled=TRUE;
	        }
	if(this_host->have_event_handler_enabled==FALSE && template_host->have_event_handler_enabled==TRUE){
		this_host->event_handler_enabled=template_host->event_handler_enabled;
		this_host->have_event_handler_enabled=TRUE;
	        }
	if(this_host->have_low_flap_threshold==FALSE && template_host->have_low_flap_threshold==TRUE){
		this_host->low_flap_threshold=template_host->low_flap_threshold;
		this_host->have_low_flap_threshold=TRUE;
	        }
	if(this_host->have_high_flap_threshold==FALSE && template_host->have_high_flap_threshold==TRUE){
		this_host->high_flap_threshold=template_host->high_flap_threshold;
		this_host->have_high_flap_threshold=TRUE;
	        }
	if(this_host->have_flap_detection_enabled==FALSE && template_host->have_flap_detection_enabled==TRUE){
		this_host->flap_detection_enabled=template_host->flap_detection_enabled;
		this_host->have_flap_detection_enabled=TRUE;
	        }
	if(this_host->have_notification_options==FALSE && template_host->have_notification_options==TRUE){
		this_host->notify_on_down=template_host->notify_on_down;
		this_host->notify_on_unreachable=template_host->notify_on_unreachable;
		this_host->notify_on_recovery=template_host->notify_on_recovery;
		this_host->have_notification_options=TRUE;
	        }
	if(this_host->have_notifications_enabled==FALSE && template_host->have_notifications_enabled==TRUE){
		this_host->notifications_enabled=template_host->notifications_enabled;
		this_host->have_notifications_enabled=TRUE;
	        }
	if(this_host->have_notification_interval==FALSE && template_host->have_notification_interval==TRUE){
		this_host->notification_interval=template_host->notification_interval;
		this_host->have_notification_interval=TRUE;
	        }
	if(this_host->have_stalking_options==FALSE && template_host->have_stalking_options==TRUE){
		this_host->stalk_on_up=template_host->stalk_on_up;
		this_host->stalk_on_down=template_host->stalk_on_down;
		this_host->stalk_on_unreachable=template_host->stalk_on_unreachable;
		this_host->have_stalking_options=TRUE;
	        }
	if(this_host->have_process_perf_data==FALSE && template_host->have_process_perf_data==TRUE){
		this_host->process_perf_data=template_host->process_perf_data;
		this_host->have_process_perf_data=TRUE;
	        }
	if(this_host->have_failure_prediction_enabled==FALSE && template_host->have_failure_prediction_enabled==TRUE){
		this_host->failure_prediction_enabled=template_host->failure_prediction_enabled;
		this_host->have_failure_prediction_enabled=TRUE;
	        }
	if(this_host->have_retain_status_information==FALSE && template_host->have_retain_status_information==TRUE){
		this_host->retain_status_information=template_host->retain_status_information;
		this_host->have_retain_status_information=TRUE;
	        }
	if(this_host->have_retain_nonstatus_information==FALSE && template_host->have_retain_nonstatus_information==TRUE){
		this_host->retain_nonstatus_information=template_host->retain_nonstatus_information;
		this_host->have_retain_nonstatus_information=TRUE;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_host() end\n");
#endif

	return OK;
        }



/* resolves a service object */
int xodtemplate_resolve_service(xodtemplate_service *this_service){
	xodtemplate_service *template_service;

#ifdef DEBUG0
	printf("xodtemplate_resolve_service() start\n");
#endif

	/* return if this service has already been resolved */
	if(this_service->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_service->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_service->template==NULL)
		return OK;

	template_service=xodtemplate_find_service(this_service->template);
	if(template_service==NULL){
#ifdef NSCORE
		printf("Error: Template '%s' specified in service '%s' definition not found!\n",this_service->template,(this_service->name==NULL)?"NULL":this_service->name);
#endif
		return ERROR;
	        }

	/* resolve the template service... */
	xodtemplate_resolve_service(template_service);

	/* apply missing properties from template service... */
	if(this_service->hostgroup_name==NULL && template_service->hostgroup_name!=NULL){
		this_service->hostgroup_name=(char *)malloc(strlen(template_service->hostgroup_name)+1);
		if(this_service->hostgroup_name!=NULL)
			strcpy(this_service->hostgroup_name,template_service->hostgroup_name);
	        }
	if(this_service->host_name==NULL && template_service->host_name!=NULL){
		this_service->host_name=(char *)malloc(strlen(template_service->host_name)+1);
		if(this_service->host_name!=NULL)
			strcpy(this_service->host_name,template_service->host_name);
	        }
	if(this_service->service_description==NULL && template_service->service_description!=NULL){
		this_service->service_description=(char *)malloc(strlen(template_service->service_description)+1);
		if(this_service->service_description!=NULL)
			strcpy(this_service->service_description,template_service->service_description);
	        }
	if(this_service->check_command==NULL && template_service->check_command!=NULL){
		this_service->check_command=(char *)malloc(strlen(template_service->check_command)+1);
		if(this_service->check_command!=NULL)
			strcpy(this_service->check_command,template_service->check_command);
	        }
	if(this_service->check_period==NULL && template_service->check_period!=NULL){
		this_service->check_period=(char *)malloc(strlen(template_service->check_period)+1);
		if(this_service->check_period!=NULL)
			strcpy(this_service->check_period,template_service->check_period);
	        }
	if(this_service->event_handler==NULL && template_service->event_handler!=NULL){
		this_service->event_handler=(char *)malloc(strlen(template_service->event_handler)+1);
		if(this_service->event_handler!=NULL)
			strcpy(this_service->event_handler,template_service->event_handler);
	        }
	if(this_service->notification_period==NULL && template_service->notification_period!=NULL){
		this_service->notification_period=(char *)malloc(strlen(template_service->notification_period)+1);
		if(this_service->notification_period!=NULL)
			strcpy(this_service->notification_period,template_service->notification_period);
	        }
	if(this_service->contact_groups==NULL && template_service->contact_groups!=NULL){
		this_service->contact_groups=(char *)malloc(strlen(template_service->contact_groups)+1);
		if(this_service->contact_groups!=NULL)
			strcpy(this_service->contact_groups,template_service->contact_groups);
	        }
	if(this_service->failure_prediction_options==NULL && template_service->failure_prediction_options!=NULL){
		this_service->failure_prediction_options=(char *)malloc(strlen(template_service->failure_prediction_options)+1);
		if(this_service->failure_prediction_options!=NULL)
			strcpy(this_service->failure_prediction_options,template_service->failure_prediction_options);
	        }
	if(this_service->have_max_check_attempts==FALSE && template_service->have_max_check_attempts==TRUE){
		this_service->max_check_attempts=template_service->max_check_attempts;
		this_service->have_max_check_attempts=TRUE;
	        }
	if(this_service->have_normal_check_interval==FALSE && template_service->have_normal_check_interval==TRUE){
		this_service->normal_check_interval=template_service->normal_check_interval;
		this_service->have_normal_check_interval=TRUE;
	        }
	if(this_service->have_retry_check_interval==FALSE && template_service->have_retry_check_interval==TRUE){
		this_service->retry_check_interval=template_service->retry_check_interval;
		this_service->have_retry_check_interval=TRUE;
	        }
	if(this_service->have_active_checks_enabled==FALSE && template_service->have_active_checks_enabled==TRUE){
		this_service->active_checks_enabled=template_service->active_checks_enabled;
		this_service->have_active_checks_enabled=TRUE;
	        }
	if(this_service->have_passive_checks_enabled==FALSE && template_service->have_passive_checks_enabled==TRUE){
		this_service->passive_checks_enabled=template_service->passive_checks_enabled;
		this_service->have_passive_checks_enabled=TRUE;
	        }
	if(this_service->have_parallelize_check==FALSE && template_service->have_parallelize_check==TRUE){
		this_service->parallelize_check=template_service->parallelize_check;
		this_service->have_parallelize_check=TRUE;
	        }
	if(this_service->have_is_volatile==FALSE && template_service->have_is_volatile==TRUE){
		this_service->is_volatile=template_service->is_volatile;
		this_service->have_is_volatile=TRUE;
	        }
	if(this_service->have_obsess_over_service==FALSE && template_service->have_obsess_over_service==TRUE){
		this_service->obsess_over_service=template_service->obsess_over_service;
		this_service->have_obsess_over_service=TRUE;
	        }
	if(this_service->have_event_handler_enabled==FALSE && template_service->have_event_handler_enabled==TRUE){
		this_service->event_handler_enabled=template_service->event_handler_enabled;
		this_service->have_event_handler_enabled=TRUE;
	        }
	if(this_service->have_check_freshness==FALSE && template_service->have_check_freshness==TRUE){
		this_service->check_freshness=template_service->check_freshness;
		this_service->have_check_freshness=TRUE;
	        }
	if(this_service->have_freshness_threshold==FALSE && template_service->have_freshness_threshold==TRUE){
		this_service->freshness_threshold=template_service->freshness_threshold;
		this_service->have_freshness_threshold=TRUE;
	        }
	if(this_service->have_low_flap_threshold==FALSE && template_service->have_low_flap_threshold==TRUE){
		this_service->low_flap_threshold=template_service->low_flap_threshold;
		this_service->have_low_flap_threshold=TRUE;
	        }
	if(this_service->have_high_flap_threshold==FALSE && template_service->have_high_flap_threshold==TRUE){
		this_service->high_flap_threshold=template_service->high_flap_threshold;
		this_service->have_high_flap_threshold=TRUE;
	        }
	if(this_service->have_flap_detection_enabled==FALSE && template_service->have_flap_detection_enabled==TRUE){
		this_service->flap_detection_enabled=template_service->flap_detection_enabled;
		this_service->have_flap_detection_enabled=TRUE;
	        }
	if(this_service->have_notification_options==FALSE && template_service->have_notification_options==TRUE){
		this_service->notify_on_unknown=template_service->notify_on_unknown;
		this_service->notify_on_warning=template_service->notify_on_warning;
		this_service->notify_on_critical=template_service->notify_on_critical;
		this_service->notify_on_recovery=template_service->notify_on_recovery;
		this_service->have_notification_options=TRUE;
	        }
	if(this_service->have_notifications_enabled==FALSE && template_service->have_notifications_enabled==TRUE){
		this_service->notifications_enabled=template_service->notifications_enabled;
		this_service->have_notifications_enabled=TRUE;
	        }
	if(this_service->have_notification_interval==FALSE && template_service->have_notification_interval==TRUE){
		this_service->notification_interval=template_service->notification_interval;
		this_service->have_notification_interval=TRUE;
	        }
	if(this_service->have_stalking_options==FALSE && template_service->have_stalking_options==TRUE){
		this_service->stalk_on_ok=template_service->stalk_on_ok;
		this_service->stalk_on_unknown=template_service->stalk_on_unknown;
		this_service->stalk_on_warning=template_service->stalk_on_warning;
		this_service->stalk_on_critical=template_service->stalk_on_critical;
		this_service->have_stalking_options=TRUE;
	        }
	if(this_service->have_process_perf_data==FALSE && template_service->have_process_perf_data==TRUE){
		this_service->process_perf_data=template_service->process_perf_data;
		this_service->have_process_perf_data=TRUE;
	        }
	if(this_service->have_failure_prediction_enabled==FALSE && template_service->have_failure_prediction_enabled==TRUE){
		this_service->failure_prediction_enabled=template_service->failure_prediction_enabled;
		this_service->have_failure_prediction_enabled=TRUE;
	        }
	if(this_service->have_retain_status_information==FALSE && template_service->have_retain_status_information==TRUE){
		this_service->retain_status_information=template_service->retain_status_information;
		this_service->have_retain_status_information=TRUE;
	        }
	if(this_service->have_retain_nonstatus_information==FALSE && template_service->have_retain_nonstatus_information==TRUE){
		this_service->retain_nonstatus_information=template_service->retain_nonstatus_information;
		this_service->have_retain_nonstatus_information=TRUE;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_service() end\n");
#endif

	return OK;
        }


/* resolves a hostdependency object */
int xodtemplate_resolve_hostdependency(xodtemplate_hostdependency *this_hostdependency){
	xodtemplate_hostdependency *template_hostdependency;

#ifdef DEBUG0
	printf("xodtemplate_resolve_hostdependency() start\n");
#endif

	/* return if this hostdependency has already been resolved */
	if(this_hostdependency->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_hostdependency->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_hostdependency->template==NULL)
		return OK;

	template_hostdependency=xodtemplate_find_hostdependency(this_hostdependency->template);
	if(template_hostdependency==NULL){
#ifdef NSCORE
		printf("Error: Template '%s' specified in hostdependency '%s' definition not found!\n",this_hostdependency->template,(this_hostdependency->name==NULL)?"NULL":this_hostdependency->name);
#endif
		return ERROR;
	        }

	/* resolve the template hostdependency... */
	xodtemplate_resolve_hostdependency(template_hostdependency);

	/* apply missing properties from template hostdependency... */
	if(this_hostdependency->host_name==NULL && template_hostdependency->host_name!=NULL){
		this_hostdependency->host_name=(char *)malloc(strlen(template_hostdependency->host_name)+1);
		if(this_hostdependency->host_name!=NULL)
			strcpy(this_hostdependency->host_name,template_hostdependency->host_name);
	        }
	if(this_hostdependency->dependent_host_name==NULL && template_hostdependency->dependent_host_name!=NULL){
		this_hostdependency->dependent_host_name=(char *)malloc(strlen(template_hostdependency->dependent_host_name)+1);
		if(this_hostdependency->dependent_host_name!=NULL)
			strcpy(this_hostdependency->dependent_host_name,template_hostdependency->dependent_host_name);
	        }
	if(this_hostdependency->have_notification_dependency_options==FALSE && template_hostdependency->have_notification_dependency_options==TRUE){
		this_hostdependency->fail_notify_on_up=template_hostdependency->fail_notify_on_up;
		this_hostdependency->fail_notify_on_down=template_hostdependency->fail_notify_on_down;
		this_hostdependency->fail_notify_on_unreachable=template_hostdependency->fail_notify_on_unreachable;
		this_hostdependency->have_notification_dependency_options=TRUE;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_hostdependency() end\n");
#endif

	return OK;
        }


/* resolves a hostescalation object */
int xodtemplate_resolve_hostescalation(xodtemplate_hostescalation *this_hostescalation){
	xodtemplate_hostescalation *template_hostescalation;

#ifdef DEBUG0
	printf("xodtemplate_resolve_hostescalation() start\n");
#endif

	/* return if this hostescalation has already been resolved */
	if(this_hostescalation->has_been_resolved==TRUE)
		return OK;

	/* set the resolved flag */
	this_hostescalation->has_been_resolved=TRUE;

	/* return if we have no template */
	if(this_hostescalation->template==NULL)
		return OK;

	template_hostescalation=xodtemplate_find_hostescalation(this_hostescalation->template);
	if(template_hostescalation==NULL){
#ifdef NSCORE
		printf("Error: Template '%s' specified in hostescalation '%s' definition not found!\n",this_hostescalation->template,(this_hostescalation->name==NULL)?"NULL":this_hostescalation->name);
#endif
		return ERROR;
	        }

	/* resolve the template hostescalation... */
	xodtemplate_resolve_hostescalation(template_hostescalation);

	/* apply missing properties from template hostescalation... */
	if(this_hostescalation->host_name==NULL && template_hostescalation->host_name!=NULL){
		this_hostescalation->host_name=(char *)malloc(strlen(template_hostescalation->host_name)+1);
		if(this_hostescalation->host_name!=NULL)
			strcpy(this_hostescalation->host_name,template_hostescalation->host_name);
	        }
	if(this_hostescalation->contact_groups==NULL && template_hostescalation->contact_groups!=NULL){
		this_hostescalation->contact_groups=(char *)malloc(strlen(template_hostescalation->contact_groups)+1);
		if(this_hostescalation->contact_groups!=NULL)
			strcpy(this_hostescalation->contact_groups,template_hostescalation->contact_groups);
	        }
	if(this_hostescalation->have_first_notification==FALSE && template_hostescalation->have_first_notification==TRUE){
		this_hostescalation->first_notification=template_hostescalation->first_notification;
		this_hostescalation->have_first_notification=TRUE;
	        }
	if(this_hostescalation->have_last_notification==FALSE && template_hostescalation->have_last_notification==TRUE){
		this_hostescalation->last_notification=template_hostescalation->last_notification;
		this_hostescalation->have_last_notification=TRUE;
	        }
	if(this_hostescalation->have_notification_interval==FALSE && template_hostescalation->have_notification_interval==TRUE){
		this_hostescalation->notification_interval=template_hostescalation->notification_interval;
		this_hostescalation->have_notification_interval=TRUE;
	        }

#ifdef DEBUG0
	printf("xodtemplate_resolve_hostdependency() end\n");
#endif

	return OK;
        }




/******************************************************************/
/******************* OBJECT SEARCH FUNCTIONS **********************/
/******************************************************************/

/* finds a specific timeperiod object */
xodtemplate_timeperiod *xodtemplate_find_timeperiod(char *name){
	xodtemplate_timeperiod *temp_timeperiod;

	if(name==NULL)
		return NULL;

	for(temp_timeperiod=xodtemplate_timeperiod_list;temp_timeperiod!=NULL;temp_timeperiod=temp_timeperiod->next){
		if(temp_timeperiod->name==NULL)
			continue;
		if(!strcmp(temp_timeperiod->name,name))
			break;
	        }

	return temp_timeperiod;
        }


/* finds a specific command object */
xodtemplate_command *xodtemplate_find_command(char *name){
	xodtemplate_command *temp_command;

	if(name==NULL)
		return NULL;

	for(temp_command=xodtemplate_command_list;temp_command!=NULL;temp_command=temp_command->next){
		if(temp_command->name==NULL)
			continue;
		if(!strcmp(temp_command->name,name))
			break;
	        }

	return temp_command;
        }


/* finds a specific contactgroup object */
xodtemplate_contactgroup *xodtemplate_find_contactgroup(char *name){
	xodtemplate_contactgroup *temp_contactgroup;

	if(name==NULL)
		return NULL;

	for(temp_contactgroup=xodtemplate_contactgroup_list;temp_contactgroup!=NULL;temp_contactgroup=temp_contactgroup->next){
		if(temp_contactgroup->name==NULL)
			continue;
		if(!strcmp(temp_contactgroup->name,name))
			break;
	        }

	return temp_contactgroup;
        }


/* finds a specific hostgroup object */
xodtemplate_hostgroup *xodtemplate_find_hostgroup(char *name){
	xodtemplate_hostgroup *temp_hostgroup;

	if(name==NULL)
		return NULL;

	for(temp_hostgroup=xodtemplate_hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){
		if(temp_hostgroup->name==NULL)
			continue;
		if(!strcmp(temp_hostgroup->name,name))
			break;
	        }

	return temp_hostgroup;
        }


/* finds a specific servicedependency object */
xodtemplate_servicedependency *xodtemplate_find_servicedependency(char *name){
	xodtemplate_servicedependency *temp_servicedependency;

	if(name==NULL)
		return NULL;

	for(temp_servicedependency=xodtemplate_servicedependency_list;temp_servicedependency!=NULL;temp_servicedependency=temp_servicedependency->next){
		if(temp_servicedependency->name==NULL)
			continue;
		if(!strcmp(temp_servicedependency->name,name))
			break;
	        }

	return temp_servicedependency;
        }


/* finds a specific serviceescalation object */
xodtemplate_serviceescalation *xodtemplate_find_serviceescalation(char *name){
	xodtemplate_serviceescalation *temp_serviceescalation;

	if(name==NULL)
		return NULL;

	for(temp_serviceescalation=xodtemplate_serviceescalation_list;temp_serviceescalation!=NULL;temp_serviceescalation=temp_serviceescalation->next){
		if(temp_serviceescalation->name==NULL)
			continue;
		if(!strcmp(temp_serviceescalation->name,name))
			break;
	        }

	return temp_serviceescalation;
        }


/* finds a specific hostgroupescalation object */
xodtemplate_hostgroupescalation *xodtemplate_find_hostgroupescalation(char *name){
	xodtemplate_hostgroupescalation *temp_hostgroupescalation;

	if(name==NULL)
		return NULL;

	for(temp_hostgroupescalation=xodtemplate_hostgroupescalation_list;temp_hostgroupescalation!=NULL;temp_hostgroupescalation=temp_hostgroupescalation->next){
		if(temp_hostgroupescalation->name==NULL)
			continue;
		if(!strcmp(temp_hostgroupescalation->name,name))
			break;
	        }

	return temp_hostgroupescalation;
        }


/* finds a specific contact object */
xodtemplate_contact *xodtemplate_find_contact(char *name){
	xodtemplate_contact *temp_contact;

	if(name==NULL)
		return NULL;

	for(temp_contact=xodtemplate_contact_list;temp_contact!=NULL;temp_contact=temp_contact->next){
		if(temp_contact->name==NULL)
			continue;
		if(!strcmp(temp_contact->name,name))
			break;
	        }

	return temp_contact;
        }


/* finds a specific host object */
xodtemplate_host *xodtemplate_find_host(char *name){
	xodtemplate_host *temp_host;

	if(name==NULL)
		return NULL;

	for(temp_host=xodtemplate_host_list;temp_host!=NULL;temp_host=temp_host->next){
		if(temp_host->name==NULL)
			continue;
		if(!strcmp(temp_host->name,name))
			break;
	        }

	return temp_host;
        }


/* finds a specific service object */
xodtemplate_service *xodtemplate_find_service(char *name){
	xodtemplate_service *temp_service;

	if(name==NULL)
		return NULL;

	for(temp_service=xodtemplate_service_list;temp_service!=NULL;temp_service=temp_service->next){
		if(temp_service->name==NULL)
			continue;
		if(!strcmp(temp_service->name,name))
			break;
	        }

	return temp_service;
        }


/* finds a specific hostdependency object */
xodtemplate_hostdependency *xodtemplate_find_hostdependency(char *name){
	xodtemplate_hostdependency *temp_hostdependency;

	if(name==NULL)
		return NULL;

	for(temp_hostdependency=xodtemplate_hostdependency_list;temp_hostdependency!=NULL;temp_hostdependency=temp_hostdependency->next){
		if(temp_hostdependency->name==NULL)
			continue;
		if(!strcmp(temp_hostdependency->name,name))
			break;
	        }

	return temp_hostdependency;
        }


/* finds a specific hostescalation object */
xodtemplate_hostescalation *xodtemplate_find_hostescalation(char *name){
	xodtemplate_hostescalation *temp_hostescalation;

	if(name==NULL)
		return NULL;

	for(temp_hostescalation=xodtemplate_hostescalation_list;temp_hostescalation!=NULL;temp_hostescalation=temp_hostescalation->next){
		if(temp_hostescalation->name==NULL)
			continue;
		if(!strcmp(temp_hostescalation->name,name))
			break;
	        }

	return temp_hostescalation;
        }




/******************************************************************/
/**************** OBJECT REGISTRATION FUNCTIONS *******************/
/******************************************************************/

/* registers object definitions */
int xodtemplate_register_objects(void){
	int result=OK;
	xodtemplate_timeperiod *temp_timeperiod;
	xodtemplate_command *temp_command;
	xodtemplate_contactgroup *temp_contactgroup;
	xodtemplate_hostgroup *temp_hostgroup;
	xodtemplate_contact *temp_contact;
	xodtemplate_host *temp_host;
	xodtemplate_service *temp_service;
	xodtemplate_servicedependency *temp_servicedependency;
	xodtemplate_serviceescalation *temp_serviceescalation;
	xodtemplate_hostdependency *temp_hostdependency;
	xodtemplate_hostescalation *temp_hostescalation;
	xodtemplate_hostgroupescalation *temp_hostgroupescalation;

#ifdef DEBUG0
	printf("xodtemplate_register_objects() start\n");
#endif

	/* register timeperiods */
	for(temp_timeperiod=xodtemplate_timeperiod_list;temp_timeperiod!=NULL;temp_timeperiod=temp_timeperiod->next){
		if((result=xodtemplate_register_timeperiod(temp_timeperiod))==ERROR)
			return ERROR;
	        }

	/* register commands */
	for(temp_command=xodtemplate_command_list;temp_command!=NULL;temp_command=temp_command->next){
		if((result=xodtemplate_register_command(temp_command))==ERROR)
			return ERROR;
	        }

	/* register contactgroups */
	for(temp_contactgroup=xodtemplate_contactgroup_list;temp_contactgroup!=NULL;temp_contactgroup=temp_contactgroup->next){
		if((result=xodtemplate_register_contactgroup(temp_contactgroup))==ERROR)
			return ERROR;
	        }

	/* register hostgroups */
	for(temp_hostgroup=xodtemplate_hostgroup_list;temp_hostgroup!=NULL;temp_hostgroup=temp_hostgroup->next){
		if((result=xodtemplate_register_hostgroup(temp_hostgroup))==ERROR)
			return ERROR;
	        }

	/* register contacts */
	for(temp_contact=xodtemplate_contact_list;temp_contact!=NULL;temp_contact=temp_contact->next){
		if((result=xodtemplate_register_contact(temp_contact))==ERROR)
			return ERROR;
	        }

	/* register hosts */
	for(temp_host=xodtemplate_host_list;temp_host!=NULL;temp_host=temp_host->next){
		if((result=xodtemplate_register_host(temp_host))==ERROR)
			return ERROR;
	        }

	/* register services */
	for(temp_service=xodtemplate_service_list;temp_service!=NULL;temp_service=temp_service->next){
		if((result=xodtemplate_register_service(temp_service))==ERROR)
			return ERROR;
	        }

	/* register service dependencies */
	for(temp_servicedependency=xodtemplate_servicedependency_list;temp_servicedependency!=NULL;temp_servicedependency=temp_servicedependency->next){
		if((result=xodtemplate_register_servicedependency(temp_servicedependency))==ERROR)
			return ERROR;
	        }

	/* register service escalations */
	for(temp_serviceescalation=xodtemplate_serviceescalation_list;temp_serviceescalation!=NULL;temp_serviceescalation=temp_serviceescalation->next){
		if((result=xodtemplate_register_serviceescalation(temp_serviceescalation))==ERROR)
			return ERROR;
	        }

	/* register host dependencies */
	for(temp_hostdependency=xodtemplate_hostdependency_list;temp_hostdependency!=NULL;temp_hostdependency=temp_hostdependency->next){
		if((result=xodtemplate_register_hostdependency(temp_hostdependency))==ERROR)
			return ERROR;
	        }

	/* register host escalations */
	for(temp_hostescalation=xodtemplate_hostescalation_list;temp_hostescalation!=NULL;temp_hostescalation=temp_hostescalation->next){
		if((result=xodtemplate_register_hostescalation(temp_hostescalation))==ERROR)
			return ERROR;
	        }

	/* register hostgroup escalations */
	for(temp_hostgroupescalation=xodtemplate_hostgroupescalation_list;temp_hostgroupescalation!=NULL;temp_hostgroupescalation=temp_hostgroupescalation->next){
		if((result=xodtemplate_register_hostgroupescalation(temp_hostgroupescalation))==ERROR)
			return ERROR;
	        }


#ifdef DEBUG0
	printf("xodtemplate_register_objects() end\n");
#endif

	return OK;
        }



/* registers a timeperiod definition */
int xodtemplate_register_timeperiod(xodtemplate_timeperiod *this_timeperiod){
	timeperiod *new_timeperiod;
	timerange *new_timerange;
	int day;
	char *day_range_ptr;
	char *day_range_start_buffer;
	char *range_ptr;
	char *range_buffer;
	char *time_ptr;
	char *time_buffer;
	int hours;
	int minutes;
	unsigned long range_start_time;
	unsigned long range_end_time;
	

#ifdef DEBUG0
	printf("xodtemplate_register_timeperiod() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_timeperiod->register_object==FALSE)
		return OK;

	/* add the timeperiod */
	new_timeperiod=add_timeperiod(this_timeperiod->timeperiod_name,this_timeperiod->alias);

	/* return with an error if we couldn't add the timeperiod */
	if(new_timeperiod==NULL){
#ifdef NSCORE
		printf("Error: Could not add timeperiod '%s'.\n",this_timeperiod->timeperiod_name);
#endif
		return ERROR;
	        }

	/* add all necessary timeranges to timeperiod */
	for(day=0;day<7;day++){

		if(this_timeperiod->timeranges[day]==NULL)
			continue;

		day_range_ptr=this_timeperiod->timeranges[day];
		for(day_range_start_buffer=my_strsep(&day_range_ptr,", ");day_range_start_buffer!=NULL;day_range_start_buffer=my_strsep(&day_range_ptr,", ")){

			range_ptr=day_range_start_buffer;
			range_buffer=my_strsep(&range_ptr,"-");
			if(range_buffer==NULL){
#ifdef NSCORE
				printf("Error: Could not add timeperiod '%s' timerange for day %d (No start time delimiter).\n",this_timeperiod->timeperiod_name,day);
#endif
				return ERROR;
			        }

			time_ptr=range_buffer;
			time_buffer=my_strsep(&time_ptr,":");
			if(time_buffer==NULL){
#ifdef NSCORE
				printf("Error: Could not add timeperiod '%s' timerange for day %d (No start time hours).\n",this_timeperiod->timeperiod_name,day);
#endif
				return ERROR;
			        }
			hours=atoi(time_buffer);
			time_buffer=my_strsep(&time_ptr,":");
			if(time_buffer==NULL){
#ifdef NSCORE
				printf("Error: Could not add timeperiod '%s' timerange for day %d (No start time minutes).\n",this_timeperiod->timeperiod_name,day);
#endif
				return ERROR;
			        }
			minutes=atoi(time_buffer);

			/* calculate the range start time in seconds */
			range_start_time=(unsigned long)((minutes*60)+(hours*60*60));

			range_buffer=my_strsep(&range_ptr,"-");
			if(range_buffer==NULL){
#ifdef NSCORE
				printf("Error: Could not add timeperiod '%s' timerange for day %d (No end time delimiter).\n",this_timeperiod->timeperiod_name,day);
#endif
				return ERROR;
			        }

			time_ptr=range_buffer;
			time_buffer=my_strsep(&time_ptr,":");
			if(time_buffer==NULL){
#ifdef NSCORE
				printf("Error: Could not add timeperiod '%s' timerange for day %d (No end time hours).\n",this_timeperiod->timeperiod_name,day);
#endif
				return ERROR;
			        }
			hours=atoi(time_buffer);

			time_buffer=my_strsep(&time_ptr,":");
			if(time_buffer==NULL){
#ifdef NSCORE
				printf("Error: Could not add timeperiod '%s' timerange for day %d (No end time minutes).\n",this_timeperiod->timeperiod_name,day);
#endif
				return ERROR;
			        }
			minutes=atoi(time_buffer);

			/* calculate the range end time in seconds */
			range_end_time=(unsigned long)((minutes*60)+(hours*3600));

			/* add the new time range to the time period */
			new_timerange=add_timerange_to_timeperiod(new_timeperiod,day,range_start_time,range_end_time);
			if(new_timerange==NULL){
#ifdef NSCORE
				printf("Error: Could not add timeperiod '%s' timerange for day %d.\n",this_timeperiod->timeperiod_name,day);
#endif
				return ERROR;
			        }
		        }
		
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_timeperiod() end\n");
#endif

	return OK;
        }



/* registers a command definition */
int xodtemplate_register_command(xodtemplate_command *this_command){
	command *new_command;

#ifdef DEBUG0
	printf("xodtemplate_register_command() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_command->register_object==FALSE)
		return OK;

	/* add the command */
	new_command=add_command(this_command->command_name,this_command->command_line);

	/* return with an error if we couldn't add the command */
	if(new_command==NULL){
#ifdef NSCORE
		printf("Error: Could not add command '%s'\n",this_command->command_name);
#endif
		return ERROR;
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_command() end\n");
#endif

	return OK;
        }



/* registers a contactgroup definition */
int xodtemplate_register_contactgroup(xodtemplate_contactgroup *this_contactgroup){
	contactgroup *new_contactgroup;
	contactgroupmember *new_contactgroupmember;
	char *contact_name;

#ifdef DEBUG0
	printf("xodtemplate_register_contactgroup() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_contactgroup->register_object==FALSE)
		return OK;

	/* add the contact group */
	new_contactgroup=add_contactgroup(this_contactgroup->contactgroup_name,this_contactgroup->alias);

	/* return with an error if we couldn't add the contactgroup */
	if(new_contactgroup==NULL){
#ifdef NSCORE
		printf("Error: Could not add contactgroup '%s'.\n",this_contactgroup->contactgroup_name);
#endif
		return ERROR;
	        }

	/* add all members to the contact group */
	if(this_contactgroup->members==NULL){
#ifdef NSCORE
		printf("Error: Contactgroup '%s' has no members.\n",this_contactgroup->contactgroup_name);
#endif
		return ERROR;
	        }
	for(contact_name=strtok(this_contactgroup->members,", ");contact_name!=NULL;contact_name=strtok(NULL,", ")){
		new_contactgroupmember=add_contact_to_contactgroup(new_contactgroup,contact_name);
		if(new_contactgroupmember==NULL){
#ifdef NSCORE
			printf("Error: Could not add contact '%s' to contactgroup '%s'\n",contact_name,this_contactgroup->contactgroup_name);
#endif
			return ERROR;
		        }
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_contactgroup() end\n");
#endif

	return OK;
        }



/* registers a hostgroup definition */
int xodtemplate_register_hostgroup(xodtemplate_hostgroup *this_hostgroup){
	hostgroup *new_hostgroup;
	hostgroupmember *new_hostgroupmember;
	contactgroupsmember *new_contactgroupsmember;
	char *host_name;
	char *contact_group;

#ifdef DEBUG0
	printf("xodtemplate_register_hostgroup() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_hostgroup->register_object==FALSE)
		return OK;

	/* add the  host group */
	new_hostgroup=add_hostgroup(this_hostgroup->hostgroup_name,this_hostgroup->alias);

	/* return with an error if we couldn't add the hostgroup */
	if(new_hostgroup==NULL){
#ifdef NSCORE
		printf("Error: Could not add hostgroup '%s'.\n",this_hostgroup->hostgroup_name);
#endif
		return ERROR;
	        }

	/* add all members to the host group */
	if(this_hostgroup->members==NULL){
#ifdef NSCORE
		printf("Error: Hostgroup '%s' has no members.\n",this_hostgroup->hostgroup_name);
#endif
		return ERROR;
	        }
	for(host_name=strtok(this_hostgroup->members,", ");host_name!=NULL;host_name=strtok(NULL,", ")){
		new_hostgroupmember=add_host_to_hostgroup(new_hostgroup,host_name);
		if(new_hostgroupmember==NULL){
#ifdef NSCORE
			printf("Error: Could not add host '%s' to hostgroup '%s'.\n",host_name,this_hostgroup->hostgroup_name);
#endif
			return ERROR;
		        }
	        }

	/* add all contact groups to the host group */
	if(this_hostgroup->contact_groups!=NULL){

		for(contact_group=strtok(this_hostgroup->contact_groups,", ");contact_group!=NULL;contact_group=strtok(NULL,", ")){
			new_contactgroupsmember=add_contactgroup_to_hostgroup(new_hostgroup,contact_group);
			if(new_contactgroupsmember==NULL){
#ifdef NSCORE
				printf("Error: Could not add contactgroup '%s' to hostgroup '%s'.\n",contact_group,this_hostgroup->hostgroup_name);
#endif
				return ERROR;
			        }
	                }
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_hostgroup() end\n");
#endif

	return OK;
        }



/* registers a servicedependency definition */
int xodtemplate_register_servicedependency(xodtemplate_servicedependency *this_servicedependency){
	servicedependency *new_servicedependency;

#ifdef DEBUG0
	printf("xodtemplate_register_servicedependency() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_servicedependency->register_object==FALSE)
		return OK;

	/* add the servicedependency */
	if(this_servicedependency->have_execution_dependency_options==TRUE){

		new_servicedependency=add_service_dependency(this_servicedependency->dependent_host_name,this_servicedependency->dependent_service_description,this_servicedependency->host_name,this_servicedependency->service_description,EXECUTION_DEPENDENCY,this_servicedependency->fail_execute_on_ok,this_servicedependency->fail_execute_on_warning,this_servicedependency->fail_execute_on_unknown,this_servicedependency->fail_execute_on_critical);

		/* return with an error if we couldn't add the servicedependency */
		if(new_servicedependency==NULL){
#ifdef NSCORE
			printf("Error: Could not add service execution dependency for service '%s' on host '%s'.\n",this_servicedependency->dependent_service_description,this_servicedependency->dependent_host_name);
#endif
			return ERROR;
		        }
	        }
	if(this_servicedependency->have_notification_dependency_options==TRUE){

		new_servicedependency=add_service_dependency(this_servicedependency->dependent_host_name,this_servicedependency->dependent_service_description,this_servicedependency->host_name,this_servicedependency->service_description,NOTIFICATION_DEPENDENCY,this_servicedependency->fail_notify_on_ok,this_servicedependency->fail_notify_on_warning,this_servicedependency->fail_notify_on_unknown,this_servicedependency->fail_notify_on_critical);

		/* return with an error if we couldn't add the servicedependency */
		if(new_servicedependency==NULL){
#ifdef NSCORE
			printf("Error: Could not add service notification dependency for service '%s' on host '%s'.\n",this_servicedependency->dependent_service_description,this_servicedependency->dependent_host_name);
#endif
			return ERROR;
		        }
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_servicedependency() end\n");
#endif

	return OK;
        }



/* registers a serviceescalation definition */
int xodtemplate_register_serviceescalation(xodtemplate_serviceescalation *this_serviceescalation){
	serviceescalation *new_serviceescalation;
	contactgroupsmember *new_contactgroupsmember;
	char *contact_group;

#ifdef DEBUG0
	printf("xodtemplate_register_serviceescalation() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_serviceescalation->register_object==FALSE)
		return OK;

	/* add the serviceescalation */
	new_serviceescalation=add_serviceescalation(this_serviceescalation->host_name,this_serviceescalation->service_description,this_serviceescalation->first_notification,this_serviceescalation->last_notification,this_serviceescalation->notification_interval);

	/* return with an error if we couldn't add the serviceescalation */
	if(new_serviceescalation==NULL){
#ifdef NSCORE
		printf("Error: Could not add notification escalation for service '%s' on host '%s'.\n",this_serviceescalation->service_description,this_serviceescalation->host_name);
#endif
		return ERROR;
	        }

	/* add the contact groups */
	if(this_serviceescalation->contact_groups==NULL){
#ifdef NSCORE
		printf("Error: Escalation entry for service '%s' on host '%s' has no contact groups.\n",this_serviceescalation->service_description,this_serviceescalation->host_name);
#endif
		return ERROR;
	        }
	for(contact_group=strtok(this_serviceescalation->contact_groups,", ");contact_group!=NULL;contact_group=strtok(NULL,", ")){
		new_contactgroupsmember=add_contactgroup_to_serviceescalation(new_serviceescalation,contact_group);
		if(new_contactgroupsmember==NULL){
#ifdef NSCORE
			printf("Error: Could not add contactgroup '%s' to escalation for service '%s' on host '%s'.\n",contact_group,this_serviceescalation->service_description,this_serviceescalation->host_name);
#endif
			return ERROR;
		        }
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_serviceescalation() end\n");
#endif

	return OK;
        }



/* registers a hostgroupescalation definition */
int xodtemplate_register_hostgroupescalation(xodtemplate_hostgroupescalation *this_hostgroupescalation){
	hostgroupescalation *new_hostgroupescalation;
	contactgroupsmember *new_contactgroupsmember;
	char *contact_group;

#ifdef DEBUG0
	printf("xodtemplate_register_hostgroupescalation() start\n");
#endif
	/* bail out if we shouldn't register this object */
	if(this_hostgroupescalation->register_object==FALSE)
		return OK;

	/* add hostgroupescalation */
	new_hostgroupescalation=add_hostgroupescalation(this_hostgroupescalation->hostgroup_name,this_hostgroupescalation->first_notification,this_hostgroupescalation->last_notification,this_hostgroupescalation->notification_interval);

	/* return with an error if we couldn't add the hostgroupescalation */
	if(new_hostgroupescalation==NULL){
#ifdef NSCORE
		printf("Error: Could not add notification escalation for hostgroup '%s'.\n",this_hostgroupescalation->hostgroup_name);
#endif
		return ERROR;
	        }

	/* add the contact groups */
	if(this_hostgroupescalation->contact_groups==NULL){
#ifdef NSCORE
		printf("Error: Escalation entry for hostgroup '%s' has no contact groups.\n",this_hostgroupescalation->hostgroup_name);
#endif
		return ERROR;
	        }
	for(contact_group=strtok(this_hostgroupescalation->contact_groups,", ");contact_group!=NULL;contact_group=strtok(NULL,", ")){
		new_contactgroupsmember=add_contactgroup_to_hostgroupescalation(new_hostgroupescalation,contact_group);
		if(new_contactgroupsmember==NULL){
#ifdef NSCORE
			printf("Error: Could not add contactgroup '%s' to escalation for hostgroup '%s'.\n",this_hostgroupescalation->hostgroup_name,contact_group);
#endif
			return ERROR;
		        }
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_hostgroupescalation() end\n");
#endif

	return OK;
        }



/* registers a contact definition */
int xodtemplate_register_contact(xodtemplate_contact *this_contact){
	contact *new_contact;
	char *command_name;
	commandsmember *new_commandsmember;

#ifdef DEBUG0
	printf("xodtemplate_register_contact() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_contact->register_object==FALSE)
		return OK;

	/* add the contact */
	new_contact=add_contact(this_contact->contact_name,this_contact->alias,this_contact->email,this_contact->pager,this_contact->service_notification_period,this_contact->host_notification_period,this_contact->notify_on_service_recovery,this_contact->notify_on_service_critical,this_contact->notify_on_service_warning,this_contact->notify_on_service_unknown,this_contact->notify_on_host_recovery,this_contact->notify_on_host_down,this_contact->notify_on_host_unreachable);

	/* return with an error if we couldn't add the contact */
	if(new_contact==NULL){
#ifdef NSCORE
		printf("Error: Could not add contact '%s'.\n",this_contact->contact_name);
#endif
		return ERROR;
	        }

	/* add all the host notification commands */
	for(command_name=strtok(this_contact->host_notification_commands,", ");command_name!=NULL;command_name=strtok(NULL,", ")){
		new_commandsmember=add_host_notification_command_to_contact(new_contact,command_name);
		if(new_commandsmember==NULL){
#ifdef NSCORE
			printf("Error: Could not add host notification command '%s' to contact '%s'.\n",command_name,this_contact->contact_name);
#endif
			return ERROR;
		        }
	        }

	/* add all the service notification commands */
	for(command_name=strtok(this_contact->service_notification_commands,", ");command_name!=NULL;command_name=strtok(NULL,", ")){
		new_commandsmember=add_service_notification_command_to_contact(new_contact,command_name);
		if(new_commandsmember==NULL){
#ifdef NSCORE
			printf("Error: Could not add service notification command '%s' to contact '%s'.\n",command_name,this_contact->contact_name);
#endif
			return ERROR;
		        }
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_contact() end\n");
#endif

	return OK;
        }



/* registers a host definition */
int xodtemplate_register_host(xodtemplate_host *this_host){
	host *new_host;
	char *parent_host;
	hostsmember *new_hostsmember;

#ifdef DEBUG0
	printf("xodtemplate_register_host() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_host->register_object==FALSE)
		return OK;

	/* add the host definition */
	new_host=add_host(this_host->host_name,this_host->alias,this_host->address,this_host->max_check_attempts,this_host->notify_on_recovery,this_host->notify_on_down,this_host->notify_on_unreachable,this_host->notification_interval,this_host->notification_period,this_host->notifications_enabled,this_host->check_command,this_host->checks_enabled,this_host->event_handler,this_host->event_handler_enabled,this_host->flap_detection_enabled,this_host->low_flap_threshold,this_host->high_flap_threshold,this_host->stalk_on_up,this_host->stalk_on_down,this_host->stalk_on_unreachable,this_host->process_perf_data,this_host->failure_prediction_enabled,this_host->failure_prediction_options,this_host->retain_status_information,this_host->retain_nonstatus_information);

	/* return with an error if we couldn't add the host */
	if(new_host==NULL){
#ifdef NSCORE
		printf("Error: Could not add host '%s'.\n",this_host->host_name);
#endif
		return ERROR;
	        }

	/* add the parent hosts */
	for(parent_host=strtok(this_host->parents,", ");parent_host!=NULL;parent_host=strtok(NULL,", ")){
		new_hostsmember=add_parent_host_to_host(new_host,parent_host);
		if(new_hostsmember==NULL){
#ifdef NSCORE
			printf("Error: Could not add parent host '%s' to host '%s'.\n",parent_host,this_host->host_name);
#endif
			return ERROR;
		        }
	        }


#ifdef DEBUG0
	printf("xodtemplate_register_host() end\n");
#endif

	return OK;
        }



/* registers a service definition */
int xodtemplate_register_service(xodtemplate_service *this_service){
	service *new_service;
	contactgroupsmember *new_contactgroupsmember;
	char *contactgroup_name;

#ifdef DEBUG0
	printf("xodtemplate_register_service() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_service->register_object==FALSE)
		return OK;

	/* add the service */
	new_service=add_service(this_service->host_name,this_service->service_description,this_service->check_period,this_service->max_check_attempts,this_service->parallelize_check,this_service->passive_checks_enabled,this_service->normal_check_interval,this_service->retry_check_interval,this_service->notification_interval,this_service->notification_period,this_service->notify_on_recovery,this_service->notify_on_unknown,this_service->notify_on_warning,this_service->notify_on_critical,this_service->notifications_enabled,this_service->is_volatile,this_service->event_handler,this_service->event_handler_enabled,this_service->check_command,this_service->active_checks_enabled,this_service->flap_detection_enabled,this_service->low_flap_threshold,this_service->high_flap_threshold,this_service->stalk_on_ok,this_service->stalk_on_warning,this_service->stalk_on_unknown,this_service->stalk_on_critical,this_service->process_perf_data,this_service->failure_prediction_enabled,this_service->failure_prediction_options,this_service->check_freshness,this_service->freshness_threshold,this_service->retain_status_information,this_service->retain_nonstatus_information,this_service->obsess_over_service);

	/* return with an error if we couldn't add the service */
	if(new_service==NULL){
#ifdef NSCORE
		printf("Error: Could not add service '%s' on host '%s'.\n",this_service->service_description,this_service->host_name);
#endif
		return ERROR;
	        }

	/* add all the contact groups to the service */
	if(this_service->contact_groups!=NULL){

		for(contactgroup_name=strtok(this_service->contact_groups,", ");contactgroup_name!=NULL;contactgroup_name=strtok(NULL,", ")){

			/* add this contactgroup to the service definition */
			new_contactgroupsmember=add_contactgroup_to_service(new_service,contactgroup_name);

			/* stop adding contact groups if we ran into an error */
			if(new_contactgroupsmember==NULL){
#ifdef NSCORE
				printf("Error: Could not add contact group '%s' to service '%s' on host '%s'.\n",contactgroup_name,this_service->service_description,this_service->host_name);
#endif
				return ERROR;
		                }
		        }
	        }


#ifdef DEBUG0
	printf("xodtemplate_register_service() end\n");
#endif

	return OK;
        }



/* registers a hostdependency definition */
int xodtemplate_register_hostdependency(xodtemplate_hostdependency *this_hostdependency){
	hostdependency *new_hostdependency;

#ifdef DEBUG0
	printf("xodtemplate_register_hostdependency() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_hostdependency->register_object==FALSE)
		return OK;

	/* add the hostdependency */
	if(this_hostdependency->have_notification_dependency_options==TRUE){

		new_hostdependency=add_host_dependency(this_hostdependency->dependent_host_name,this_hostdependency->host_name,NOTIFICATION_DEPENDENCY,this_hostdependency->fail_notify_on_up,this_hostdependency->fail_notify_on_down,this_hostdependency->fail_notify_on_unreachable);

		/* return with an error if we couldn't add the hostdependency */
		if(new_hostdependency==NULL){
#ifdef NSCORE
			printf("Error: Could not add host notification dependency for host '%s'.\n",this_hostdependency->dependent_host_name);
#endif
			return ERROR;
                        }
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_hostdependency() end\n");
#endif

	return OK;
        }



/* registers a hostescalation definition */
int xodtemplate_register_hostescalation(xodtemplate_hostescalation *this_hostescalation){
	hostescalation *new_hostescalation;
	contactgroupsmember *new_contactgroupsmember;
	char *contact_group;

#ifdef DEBUG0
	printf("xodtemplate_register_hostescalation() start\n");
#endif

	/* bail out if we shouldn't register this object */
	if(this_hostescalation->register_object==FALSE)
		return OK;

	/* add the hostescalation */
	new_hostescalation=add_hostescalation(this_hostescalation->host_name,this_hostescalation->first_notification,this_hostescalation->last_notification,this_hostescalation->notification_interval);

	/* return with an error if we couldn't add the hostescalation */
	if(new_hostescalation==NULL){
#ifdef NSCORE
		printf("Error: Could not add notification escalation for host '%s'.\n",this_hostescalation->host_name);
#endif
		return ERROR;
	        }

	/* add the contact groups */
	if(this_hostescalation->contact_groups==NULL){
#ifdef NSCORE
		printf("Error: Escalation entry for host '%s' has no contact groups.\n",this_hostescalation->host_name);
#endif
		return ERROR;
	        }
	for(contact_group=strtok(this_hostescalation->contact_groups,", ");contact_group!=NULL;contact_group=strtok(NULL,", ")){
		new_contactgroupsmember=add_contactgroup_to_hostescalation(new_hostescalation,contact_group);
		if(new_contactgroupsmember==NULL){
#ifdef NSCORE
			printf("Error: Could not add contactgroup '%s' to escalation for host '%s'.\n",contact_group,this_hostescalation->host_name);
#endif
			return ERROR;
		        }
	        }

#ifdef DEBUG0
	printf("xodtemplate_register_hostescalation() end\n");
#endif

	return OK;
        }




/******************************************************************/
/********************** CLEANUP FUNCTIONS *************************/
/******************************************************************/

/* frees memory */
int xodtemplate_free_memory(void){
	xodtemplate_timeperiod *this_timeperiod;
	xodtemplate_timeperiod *next_timeperiod;
	xodtemplate_command *this_command;
	xodtemplate_command *next_command;
	xodtemplate_contactgroup *this_contactgroup;
	xodtemplate_contactgroup *next_contactgroup;
	xodtemplate_hostgroup *this_hostgroup;
	xodtemplate_hostgroup *next_hostgroup;
	xodtemplate_servicedependency *this_servicedependency;
	xodtemplate_servicedependency *next_servicedependency;
	xodtemplate_serviceescalation *this_serviceescalation;
	xodtemplate_serviceescalation *next_serviceescalation;
	xodtemplate_hostgroupescalation *this_hostgroupescalation;
	xodtemplate_hostgroupescalation *next_hostgroupescalation;
	xodtemplate_contact *this_contact;
	xodtemplate_contact *next_contact;
	xodtemplate_host *this_host;
	xodtemplate_host *next_host;
	xodtemplate_service *this_service;
	xodtemplate_service *next_service;
	xodtemplate_hostdependency *this_hostdependency;
	xodtemplate_hostdependency *next_hostdependency;
	xodtemplate_hostescalation *this_hostescalation;
	xodtemplate_hostescalation *next_hostescalation;
	int x;

#ifdef DEBUG0
	printf("xodtemplate_free_memory() start\n");
#endif

	/* free memory allocated to timeperiod list */
	for(this_timeperiod=xodtemplate_timeperiod_list;this_timeperiod!=NULL;this_timeperiod=next_timeperiod){
		next_timeperiod=this_timeperiod->next;
		free(this_timeperiod->template);
		free(this_timeperiod->name);
		free(this_timeperiod->timeperiod_name);
		free(this_timeperiod->alias);
		for(x=0;x<7;x++)
			free(this_timeperiod->timeranges[x]);
		free(this_timeperiod);
	        }

	/* free memory allocated to command list */
	for(this_command=xodtemplate_command_list;this_command!=NULL;this_command=next_command){
		next_command=this_command->next;
		free(this_command->template);
		free(this_command->name);
		free(this_command->command_name);
		free(this_command->command_line);
		free(this_command);
	        }

	/* free memory allocated to contactgroup list */
	for(this_contactgroup=xodtemplate_contactgroup_list;this_contactgroup!=NULL;this_contactgroup=next_contactgroup){
		next_contactgroup=this_contactgroup->next;
		free(this_contactgroup->template);
		free(this_contactgroup->name);
		free(this_contactgroup->contactgroup_name);
		free(this_contactgroup->alias);
		free(this_contactgroup->members);
		free(this_contactgroup);
	        }

	/* free memory allocated to hostgroup list */
	for(this_hostgroup=xodtemplate_hostgroup_list;this_hostgroup!=NULL;this_hostgroup=next_hostgroup){
		next_hostgroup=this_hostgroup->next;
		free(this_hostgroup->template);
		free(this_hostgroup->name);
		free(this_hostgroup->hostgroup_name);
		free(this_hostgroup->alias);
		free(this_hostgroup->members);
		free(this_hostgroup->contact_groups);
		free(this_hostgroup);
	        }

	/* free memory allocated to servicedependency list */
	for(this_servicedependency=xodtemplate_servicedependency_list;this_servicedependency!=NULL;this_servicedependency=next_servicedependency){
		next_servicedependency=this_servicedependency->next;
		free(this_servicedependency->template);
		free(this_servicedependency->name);
		free(this_servicedependency->host_name);
		free(this_servicedependency->service_description);
		free(this_servicedependency->dependent_host_name);
		free(this_servicedependency->dependent_service_description);
		free(this_servicedependency);
	        }

	/* free memory allocated to serviceescalation list */
	for(this_serviceescalation=xodtemplate_serviceescalation_list;this_serviceescalation!=NULL;this_serviceescalation=next_serviceescalation){
		next_serviceescalation=this_serviceescalation->next;
		free(this_serviceescalation->template);
		free(this_serviceescalation->name);
		free(this_serviceescalation->host_name);
		free(this_serviceescalation->service_description);
		free(this_serviceescalation->contact_groups);
		free(this_serviceescalation);
	        }

	/* free memory allocated to hostgroupescalation list */
	for(this_hostgroupescalation=xodtemplate_hostgroupescalation_list;this_hostgroupescalation!=NULL;this_hostgroupescalation=next_hostgroupescalation){
		next_hostgroupescalation=this_hostgroupescalation->next;
		free(this_hostgroupescalation->template);
		free(this_hostgroupescalation->name);
		free(this_hostgroupescalation->hostgroup_name);
		free(this_hostgroupescalation->contact_groups);
		free(this_hostgroupescalation);
	        }

	/* free memory allocated to contact list */
	for(this_contact=xodtemplate_contact_list;this_contact!=NULL;this_contact=next_contact){
		next_contact=this_contact->next;
		free(this_contact->template);
		free(this_contact->name);
		free(this_contact->contact_name);
		free(this_contact->alias);
		free(this_contact->email);
		free(this_contact->pager);
		free(this_contact->service_notification_period);
		free(this_contact->service_notification_commands);
		free(this_contact->host_notification_period);
		free(this_contact->host_notification_commands);
		free(this_contact);
	        }

	/* free memory allocated to host list */
	for(this_host=xodtemplate_host_list;this_host!=NULL;this_host=next_host){
		next_host=this_host->next;
		free(this_host->template);
		free(this_host->name);
		free(this_host->host_name);
		free(this_host->alias);
		free(this_host->address);
		free(this_host->parents);
		free(this_host->check_command);
		free(this_host->event_handler);
		free(this_host->notification_period);
		free(this_host->failure_prediction_options);
		free(this_host);
	        }

	/* free memory allocated to service list */
	for(this_service=xodtemplate_service_list;this_service!=NULL;this_service=next_service){
		next_service=this_service->next;
		free(this_service->template);
		free(this_service->name);
		free(this_service->hostgroup_name);
		free(this_service->host_name);
		free(this_service->service_description);
		free(this_service->check_command);
		free(this_service->check_period);
		free(this_service->event_handler);
		free(this_service->notification_period);
		free(this_service->contact_groups);
		free(this_service->failure_prediction_options);
		free(this_service);
	        }

	/* free memory allocated to hostdependency list */
	for(this_hostdependency=xodtemplate_hostdependency_list;this_hostdependency!=NULL;this_hostdependency=next_hostdependency){
		next_hostdependency=this_hostdependency->next;
		free(this_hostdependency->template);
		free(this_hostdependency->name);
		free(this_hostdependency->host_name);
		free(this_hostdependency->dependent_host_name);
		free(this_hostdependency);
	        }

	/* free memory allocated to hostescalation list */
	for(this_hostescalation=xodtemplate_hostescalation_list;this_hostescalation!=NULL;this_hostescalation=next_hostescalation){
		next_hostescalation=this_hostescalation->next;
		free(this_hostescalation->template);
		free(this_hostescalation->name);
		free(this_hostescalation->hostgroup_name);
		free(this_hostescalation->host_name);
		free(this_hostescalation->contact_groups);
		free(this_hostescalation);
	        }


#ifdef DEBUG0
	printf("xodtemplate_free_memory() end\n");
#endif

	return OK;
        }
