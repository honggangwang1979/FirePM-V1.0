/****************************************************************************************************************************************************
 *  Author: Honggang Wang and Professor Dembsey, the Department of Fire Protection Engineering in WPI
 *  Copyright: This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation with the author information being cited
 *
 *  Discription: this file includes the source code of the tool, GenFiles, which can be used to generate a number of FDS files based on the
 *  specification input by the user in the input file (SM_Info.txt, in our test case ); 
 *
 *  How to Run this tool: ./GenFiles SM_Info.txt
 *
 *  Flowchat: 
 *     step 1 -> read information from user's input file (SM_Info.txt) into  a SMInfo struct array (FDS_SmInfo)
 *     step 2 -> calculate and record how many files should be generated and store the information into a  GenInfo struct Array (FDS_GenInfo)
 *     step 3 -> Output files needed by the user to different folders based on their variables type (VarType). 
 *               VarType[0] = 'I' or 'O',  'I' means input variable, 'O' means output variables. in this file only 'I' is addressed
 *               VarType[1] = 'S' or 'R' or '_',  'S' means the thereby generated files is going to be used in sensitivity  Matrix Method (SMM);
 *                          'R' means the thereby generated files is going to be used in response surface model (RSM) or substitue model
 *                          '_' is reserved for future use or no special meanings
 *               VarType[2] = 'G' or 'P' or 'C',  'G' means the variables to be changed are geograpic variables including 6 coordinates 
 *                          'p' means the variables to be changed are  physical variables, usually with the type of double
 *                          'C' means the variables to be changed are combined variables of type 'P' or/and type 'G', only one file will be 
 *                              generated including all the changes indicated by each entries with VarType of "I*C". the combined changes case is 
 *                              used to check the accuracy of the SMM or RSM. Here the star "*" can be any char that is allowed in the filename in 
 *                              one operation system 
 *     step 4 -> refine the output files, mainly including changing the CHID and RENDER_FILE in the fds files
 ***************************************************************************************************************************************************/

#include "FirePM.h"

/************************************************************************************************************************************************* 
 * Function:  this function is a core function used to generate a number of files based on _gi and basefile _fin
 *
 *    FlowChart: 1. open the baselien FDS file for read. the baseline FDS file exists as a mother FDS file which will deliver many children FDS files with specific modifications denoted by information in _gi
 *               2.  call AlignFDS() to compact the original FDS file content and save to three Arraies in which one FDs record only takes up one line
 *               3. Call GenNewDir() to make new dirs to hold the new derived FDS files. the rule is to make sure one VarType has one independent DIR
 *               4. Call OutputCmbFiles() to create FDS files of combined fire scenarios(more than one input variables change at the same time)
 *               5. Call OutputSRFiles() to create FDS files to do SMM(sensitivity matrix method) and RSM(response surface method)  analysis 
 *               6. put the ./base directiory to the NewDir list since our following analysis will searfh this NewDir list
 *               7. for each directory in the NewDir list, refine the FDS files under this directory (make the fDs file name compatible with the CHID in the FDS file. Since the CHID cannot have dot ("."), a capital D is used. Also in this step a Mfds.sh is created as an example to run MPI version of FDS. Users may need to modify this shell based on their own machine configurations
 *
 * _fin: input parameter indicating the name of a baseline FDS file
 * _gi: input parameter indicating a GenInfo structure array which has entries of files needed to be created 
 * Return: 0: success
 *         -1: Failure 
 *************************************************************************************************************************************************/
int OutputFiles( char *_fin , struct GenInfo *_gi )
{
    char InfoArray[MAXLINENUM][MAXSTRINGSIZE];
    char medium_InfoArray[MAXLINENUM][MAXSTRINGSIZE];
    char original_InfoArray[MAXLINENUM][MAXSTRINGSIZE];
    char NewDir[MAXNEWDIRNUM][MAXSTRINGSIZE];
    FILE *fp_in = NULL; 
    int i=0,j=0,k=0;

    memset(InfoArray, '\0', sizeof (InfoArray));
    memset(medium_InfoArray, '\0', sizeof (medium_InfoArray));
    memset(original_InfoArray, '\0', sizeof (original_InfoArray));
    memset(NewDir, '\0', sizeof (NewDir));

    fp_in= fopen(_fin,"r");
    if( fp_in == NULL)
    {
       printf( "fopen wrong!\n");
       printf( _fin);
       printf("\n");
       return -1;
    }

    // if a FDS record occupies multi lines, compact them to one line. Then copy the compacted lines to InfoArray, medium_InfoArray and original_InfoArray
    if( AlignFDS(fp_in, InfoArray, medium_InfoArray, original_InfoArray) != 0 )
    {
        printf( "AlignFDS() error !\n" );
        fclose(fp_in);
        return -1;
    }
    // create directories to store files in the current directory
    if( GenNewDir(_gi,NewDir) != 0 )
    {
        printf( "GenNewDir() error!\n" );
        fclose(fp_in);
        return -1;
    } 

    // the next two function calls are the key part of this function, it generate files from _gi. if the VarType is "I_C", which means combined variables
    // changes, only one fire is generated, otherwise multiply files will be generated based on the entries in _gi, which in turn is controled 
    // by the  the user input file (SM_info.txt, in our example case) 

    // dealing with the combined fire scenario case, namely VarType[2] = 'C' 
    if( OutputCmbFiles(_gi, InfoArray, medium_InfoArray, original_InfoArray) != 0 )
    {
        printf( "OutputCmbFiles() error !\n" );
        fclose(fp_in);
        return -1;
    }

    // dealing with the SMT and RSM case, namely VarType[1] = 'S'  or 'R'
    if( OutputSRFiles( original_InfoArray, _gi ) != 0 )
    {
        printf( "OutputSRFiles() error !\n" );
        fclose(fp_in);
        return -1;
    }

    fclose(fp_in);

    for( k=0;k<MAXNEWDIRNUM; k++) // add base directory to the directory list (NewDir)
    {
         if(strlen(NewDir[k]) == 0 )
         {
            if( getcwd(NewDir[k], sizeof(NewDir[k]) ) == NULL ) 
            {
                printf( "getcwd error!\n" );
                return -1;
            }
            strcat(NewDir[k],"/");
            strcat(NewDir[k],"base");
            break;
         }
    }
    
    remove("Mfds.sh");
    for( k=0;k<MAXNEWDIRNUM; k++) // align the CHID and RENDER_FILE entries in FDS with the produced fds file name
    {
        printf( "NewDir[%d]=%s\n", k, NewDir[k] );
        if( strlen(NewDir[k]) == 0 )
            break;
        else {
            if( refinefile(NewDir[k], "Mfds.sh") != 0 )
            {
                printf("refinefile error:%s\n", NewDir[k] ); 
                return -1;
            }
        }
    } 
    
    return 0;
}

/************************************************************************************************************************************************* 
 * Function: this function is used to align the FDS file name and the entry items of CHID and RENDER_FILE in the FDS file. All the FDS files in the _NewDir will be processed.
 * _NewDir: input parameter indicating a directory holding FDs files to be refined
 * _fn_file: input parameter indicating a shell file "Mfds.sh" to be created in the _NewDir
 * Return: 0: success
 *         -1: Failure
 *************************************************************************************************************************************************/
int refinefile ( char * _NewDir, char *_fn_file)
{
    DIR *tmp_dir = NULL;
    struct dirent *en = NULL;

    char Info[MAXSTRINGSIZE];
    int i=0,j=0;

    printf( "1, _NewDir is : %s\n", _NewDir);
    memset( Info, '\0', sizeof(Info));

    tmp_dir = opendir(_NewDir); //open directory
    if (tmp_dir != NULL) 
    {
      char tmp_fn_dir[1024];
      FILE *filename_fp = NULL;
      FILE *filename_fp_dir = NULL;

      memset( tmp_fn_dir, 0x0, sizeof(tmp_fn_dir));

      filename_fp = fopen( _fn_file, "a+" ); // filename_fp point to the Mfds.sh in the same director as where the exe files are
      if( filename_fp == NULL ){
          printf( "fopen() error: filename[%s]", _fn_file ); 
          return -1;
      }

      sprintf( tmp_fn_dir, "%s/%s", _NewDir, _fn_file);
      remove(tmp_fn_dir); //if the shell file exists, delete it
      filename_fp_dir = fopen( tmp_fn_dir, "a+" ); //filename_fp_dir points to the Mfds.sh in _NewDir
      if( filename_fp_dir == NULL ){
          printf( "fopen() error: filename[%s]",tmp_fn_dir); 
          fclose(filename_fp);
          return -1;
      }

      while ((en = readdir(tmp_dir)) != NULL) 
      {
         FILE *fp = NULL;
         FILE *tmp_fp = NULL;
         char tmp_fn[MAXSTRINGSIZE];
         char tmp_whole_fn[MAXSTRINGSIZE];

         memset( tmp_fn, 0x0, sizeof(tmp_fn));
         memset( tmp_whole_fn, 0x0, sizeof(tmp_whole_fn));
         printf( "%s\n", en->d_name ); //print all directory name

         if( strstr( en->d_name, ".fds") == NULL ) //no action for non-fds files
         {
             printf( "en->d_name=%s\n", en->d_name );
             continue;
         }

         sprintf( tmp_whole_fn, "%s/%s", _NewDir, en->d_name );
         fp=fopen(tmp_whole_fn,"r"); // open one FDs file by the whole name
         if( fp == NULL )
         {
             printf( "open file [%s]failed!\n",tmp_whole_fn);
             fclose(filename_fp);
             return -1;
         }

         sprintf( tmp_fn, "%s/tmp_refine.fds", _NewDir );
         tmp_fp=fopen(tmp_fn, "w+" );  //open a tmp fds file
         if( tmp_fp == NULL )
         {
             printf( "open file [%s]failed!\n", tmp_fn );
             fclose(fp);
             fclose(filename_fp);
             return -1;
         }
         while( fgets(Info, sizeof(Info), fp) != NULL ) //for each line in the FDS file
         {
             int i = 0;
             char *tmp_head=NULL;
             char *tmp_tail=NULL;
             char tmp_first[MAXSTRINGSIZE], tmp_second[MAXSTRINGSIZE], tmp_third[MAXSTRINGSIZE];

             memset( tmp_first, '\0', sizeof(tmp_first));
             memset( tmp_second, '\0', sizeof(tmp_second));
             memset( tmp_third, '\0', sizeof(tmp_third));
             
             tmp_head = strstr(Info, ".fds" ); 
             if( tmp_head != NULL ) // if there is a fds name line in the fds file, change it with the fds name
             {
                sprintf(Info, "%s\r\n", en->d_name );
                fputs(Info, tmp_fp); 
                continue;
             }
             
             tmp_head = strstr(Info, "CHID" ); //dealing with the line including 'CHID' in FDS 
             if( tmp_head != NULL )
             {
                tmp_head = strstr(tmp_head, "\'" );
                if( tmp_head != NULL )
                {
                    tmp_head ++;
                    sprintf( tmp_second, "%s", tmp_head); // copy the left of Info to tmp_second
                    tmp_head[0] = '\0'; // seperate the first part of Info
                    sprintf( tmp_first, "%s", Info ); // get the  tmp_first

                    tmp_tail = strstr( tmp_second, "\'");
                    if( tmp_tail != NULL )
                    {
                        sprintf( tmp_third, "%s", tmp_tail ); // copy the left of tmp_second to tmp_third
                    }else{
                        printf( "CHID line in FDS file error: no ' exists, tmp_second = %s\n", tmp_second );
                        fclose(fp);
                        fclose(tmp_fp);
                        fclose(filename_fp);
                        fclose(filename_fp_dir);
                        return -1;
                    }

                    memset( tmp_second, 0x0, sizeof(tmp_second));
                    sprintf( tmp_second, "%s", en->d_name ); // replance tmp_second with FDS file name 
                    tmp_tail = NULL;
                    tmp_tail = strstr( tmp_second, ".fds" );
                    if( tmp_tail == NULL )
                    {
                        printf( "tmp_second doesn't include .fds, tmp_second = %s\n", tmp_second );
                        fclose(fp);
                        fclose(tmp_fp);
                        fclose(filename_fp);
                        fclose(filename_fp_dir);
                        return -1;
                    }else{
                        tmp_tail[0] = '\0';    // seperate the tmp_second to produce the CHID
                    }

                    memset(Info, 0x0, sizeof(Info));
                    for( i=0; i<strlen(tmp_second); i++ )
                    {
                       if( tmp_second[i]== '.' ) // CHID cannot include '.' so replace it with 'D'
                           tmp_second[i] = 'D';
                    }
                    sprintf( Info, "%s%s%s", tmp_first,  tmp_second, tmp_third );
                    fputs(Info, tmp_fp); 

                    printf( "in refinefile : Info=[%s], tmp_first=[%s], tmp_second=[%s], tmp_third=[%s]\n", Info, tmp_first, tmp_second, tmp_third);

                    continue;
                }
             }

             tmp_head = NULL;
             tmp_head = strstr(Info, "RENDER_FILE" ); //dealing with the line including 'RENDER_FILE' in FDS 
             if( tmp_head != NULL )
             {
                tmp_head = strstr(tmp_head, "\'" );
                if( tmp_head != NULL )
                {
                    tmp_head ++;
                    sprintf( tmp_second, "%s", tmp_head); // copy the left of Info to tmp_second
                    tmp_head[0] = '\0'; // seperate the first part of Info
                    sprintf( tmp_first, "%s", Info ); // copy the whole Info to tmp_first

                    tmp_tail = strstr( tmp_second, ".ge1");
                    if( tmp_tail != NULL )
                    {
                        sprintf( tmp_third, "%s", tmp_tail ); // copy the left of tmp_second to tmp_third
                    }else{
                        printf( "RENDER_FILE line in FDS file error: no .gel  exists, tmp_second = %s\n", tmp_second );
                        fclose(fp);
                        fclose(tmp_fp);
                        fclose(filename_fp);
                        fclose(filename_fp_dir);
                        return -1;
                    }

                    memset( tmp_second, 0x0, sizeof(tmp_second));
                    sprintf( tmp_second, "%s", en->d_name ); // replance tmp_second with FDS file name 
                    tmp_tail = NULL;
                    tmp_tail = strstr( tmp_second, ".fds" );
                    if( tmp_tail == NULL )
                    {
                        printf( "tmp_second doesn't include .fds, tmp_second = %s\n", tmp_second );
                        fclose(fp);
                        fclose(tmp_fp);
                        fclose(filename_fp);
                        fclose(filename_fp_dir);
                        return -1;
                    }else{
                        tmp_tail[0] = '\0';    // seperate the tmp_second to produce the CHID
                    }

                    memset(Info, 0x0, sizeof(Info));
                    sprintf( Info, "%s%s%s", tmp_first,  tmp_second, tmp_third );
                    fputs(Info, tmp_fp); 

                    printf( "in refinefile : Info=[%s]\n", Info);
                    continue;
                }
             }

             fputs(Info, tmp_fp);  // copy other unchanged lines

         }

         fclose(fp);
         fclose(tmp_fp);
         
         if( rename(tmp_fn, tmp_whole_fn) != 0 ) {
             printf( "rename error: tmpfn = [%s], tmp_whole_fn=[%s]\n", tmp_fn, tmp_whole_fn );
             closedir(tmp_dir); //close all directory
             fclose(filename_fp);
             fclose(filename_fp_dir);
             return -1;
         } else {
             char tmp_str_fn[1024];
             memset(tmp_str_fn, 0x0, sizeof(tmp_str_fn));
             sprintf( tmp_str_fn, "sfds -n 2 -T 2 -t 40:40 -p long -i %s\n", tmp_whole_fn );
             fputs(tmp_str_fn, filename_fp);
             fputs(tmp_str_fn, filename_fp_dir);
             memset(tmp_whole_fn, 0x0, sizeof(tmp_whole_fn));
         }
      }
    
      fclose(filename_fp);
      fclose(filename_fp_dir);
      closedir(tmp_dir); //close all directory

    } else {
       printf( "opendir error:%s\n", _NewDir );
       return -1;
    }
    return 0;
}


/************************************************************************************************************************************************* 
 * Function: generate information of files to be created (_gi) from the configuration file formed array (_si)
 *   Flowchat: 1. for each entry in the configuration file ( _si[i] )
 *             2. only precess the input entries ( _si[i].VarType[0] == 'I' )
 *             3. deal with the graphic record ( _si[i].VarType[2] == 'G' )
 *                for each record in _si, the number of files to be created is 2 + aoti(_si[i].Division. here 2 means lowerlimit and upperlimit
 *             4. deal with the physical record ( _si[i].VarType[2] == 'P' )
 *                for each record in _si, the number of files to be created is 2 + aoti(_si[i].Division. here 2 means lowerlimit and upperlimit
 *             5. deal with the conbined record ( _si[i].VarType[2] == 'c' )
 *                if Division='-1', the value of Lowerlimit is taken, if Division = '1', the value of UpperLimit is taken. Finally when files are created in OutputFiles(), all the records in _gi who have the same Vartype  of this type (VarType[2]='C') will form one file.
 *             6. print the  information of files to be created (_gi)
 * _si: input parameter indicating the content of the configuration file
 * _gi: output parameter houlding the records based on which many FDS files will be created
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int GenFiles(  struct SMInfo * _si, struct GenInfo * _gi )
{
    int i=0,j=0,r_si=0;
    struct ThreeDCoordinate tmp_3DC[3];

    while(strlen(trim(_si[i].VarType,NULL)) != 0 ) //for every entry in the configuration file
    {

         if( _si[i].VarType[0] != 'I' )
         {
             i++;
             printf( "i=%d\n", i);
             continue;
         } 

         if( _si[i].VarType[2] == 'G' ) // 'G' for Geograpic variables
         {
             memset( tmp_3DC, '\0', sizeof(tmp_3DC));

             sscanf(_si[i].BaseValue,"%lf|%lf|%lf|%lf|%lf|%lf", 
                    &(tmp_3DC[0].x1), &(tmp_3DC[0].x2), &(tmp_3DC[0].y1), &(tmp_3DC[0].y2), &(tmp_3DC[0].z1), &(tmp_3DC[0].z2)); 
             sscanf(_si[i].LowerLimit,"%lf|%lf|%lf|%lf|%lf|%lf", 
                    &(tmp_3DC[1].x1), &(tmp_3DC[1].x2), &(tmp_3DC[1].y1), &(tmp_3DC[1].y2), &(tmp_3DC[1].z1), &(tmp_3DC[1].z2)); 
             sscanf(_si[i].UpperLimit,"%lf|%lf|%lf|%lf|%lf|%lf", 
                    &(tmp_3DC[2].x1), &(tmp_3DC[2].x2), &(tmp_3DC[2].y1), &(tmp_3DC[2].y2), &(tmp_3DC[2].z1), &(tmp_3DC[2].z2)); 
             
             // check if the input data are correct
             if( tmp_3DC[0].x1 < tmp_3DC[1].x1 || tmp_3DC[0].x1 > tmp_3DC[2].x1 )
             {
                 printf( "baseline coordinates %f must be between the LowerLimit %f and UpperLimit %f!\n", 
                          tmp_3DC[0].x1, tmp_3DC[1].x1, tmp_3DC[2].x1 );
                 return -1;
             }
             if( tmp_3DC[0].x2 < tmp_3DC[1].x2 || tmp_3DC[0].x2 > tmp_3DC[2].x2 )
             {
                 printf( "baseline coordinates %f must be between the LowerLimit %f and UpperLimit %f!\n", 
                          tmp_3DC[0].x2, tmp_3DC[1].x2, tmp_3DC[2].x2 );
                 return -1;
             }
             if( tmp_3DC[0].y1 < tmp_3DC[1].y1 || tmp_3DC[0].y1 > tmp_3DC[2].y1 )
             {
                 printf( "baseline coordinates %f must be between the LowerLimit %f and UpperLimit %f!\n", 
                          tmp_3DC[0].y1, tmp_3DC[1].y1, tmp_3DC[2].y1 );
                 return -1;
             }
             if( tmp_3DC[0].y2 < tmp_3DC[1].y2 || tmp_3DC[0].y2 > tmp_3DC[2].y2 )
             {
                 printf( "baseline coordinates %f must be between the LowerLimit %f and UpperLimit %f!\n", 
                          tmp_3DC[0].y2, tmp_3DC[1].y2, tmp_3DC[2].y2 );
                 return -1;
             }
             if( tmp_3DC[0].z1 < tmp_3DC[1].z1 || tmp_3DC[0].z1 > tmp_3DC[2].z1 )
             {
                 printf( "baseline coordinates %f must be between the LowerLimit %f and UpperLimit %f!\n", 
                          tmp_3DC[0].z1, tmp_3DC[1].z1, tmp_3DC[2].z1 );
                 return -1;
             }
             if( tmp_3DC[0].z2 < tmp_3DC[1].z2 || tmp_3DC[0].z2 > tmp_3DC[2].z2 )
             {
                 printf( "baseline coordinates %f must be between the LowerLimit %f and UpperLimit %f!\n", 
                          tmp_3DC[0].z2, tmp_3DC[1].z2, tmp_3DC[2].z2 );
                 return -1;
             }

             if( fabs(tmp_3DC[1].x1- tmp_3DC[2].x1 ) < ZERO && fabs(tmp_3DC[1].y1- tmp_3DC[2].y1 ) < ZERO 
                 && fabs(tmp_3DC[1].z1- tmp_3DC[2].z1 ) < ZERO && fabs(tmp_3DC[1].x2- tmp_3DC[2].x2 ) < ZERO 
                 && fabs(tmp_3DC[1].y2- tmp_3DC[2].y2 ) < ZERO && fabs(tmp_3DC[1].z2- tmp_3DC[2].z2 ) < ZERO)      
             { // lowerlimit is identical to the upperlimit
                 printf( "LowerLimit %s = UpperLimit %s!\n", _si[i].LowerLimit, _si[i].UpperLimit );
                 return -1;
             }

             if( atoi(_si[i].Divisions) < 0 )
             {
                 printf( "Divisions %s must not be negative!\n", _si[i].Divisions);
                 return -1;
             }
 
             // only changes in one dimension at one time are considered, don't surrport changes in 2 or more  dimensions simultaneously
             // for each entry with VarType[2]='G', the number of files to be created is two (lowerlimit and upperlimit) plus atoi(_si[i].Divisions) 
             if ( tmp_3DC[1].x1 < tmp_3DC[2].x1 ) {
                 double step = (tmp_3DC[2].x1 - tmp_3DC[1].x1)/(atoi(_si[i].Divisions)+1);
                 double tmp_var = tmp_3DC[1].x1;

                 while ( tmp_var < tmp_3DC[2].x1 || fabs( tmp_var - tmp_3DC[2].x1) < ZERO )
                 {
                     sprintf(_gi[j].VarType, "%s", _si[i].VarType) ;
                     sprintf(_gi[j].Alias, "%s", _si[i].Alias) ;
                     sprintf(_gi[j].FDS_ID, "%s", _si[i].FDS_ID) ;
                     sprintf(_gi[j].FileVarName, "%s", _si[i].FileVarName) ;
                     sprintf(_gi[j].BaseValue, "%s", _si[i].BaseValue) ;
                     sprintf(_gi[j].MoveTo, "%.4lf|%.4lf|%.4lf|%.4lf|%.4lf|%.4lf", tmp_var, tmp_3DC[1].x2, tmp_3DC[1].y1, tmp_3DC[1].y2, tmp_3DC[1].z1, tmp_3DC[1].z2); 
                     tmp_var += step;

                     if ( j++ == MAXFILENUM-1 )
                     {
                        printf( " the files to be generated is too many!\n");
                        return -1;
                     }
                 }
             } else if ( tmp_3DC[1].y1 < tmp_3DC[2].y1 ){
                 double step = (tmp_3DC[2].y1 - tmp_3DC[1].y1)/(atoi(_si[i].Divisions)+1);
                 double tmp_var = tmp_3DC[1].y1;

                 while ( tmp_var < tmp_3DC[2].y1 || fabs( tmp_var - tmp_3DC[2].y1) < ZERO )
                 {
                     sprintf(_gi[j].VarType, "%s", _si[i].VarType) ;
                     sprintf(_gi[j].Alias, "%s", _si[i].Alias) ;
                     sprintf(_gi[j].FDS_ID, "%s", _si[i].FDS_ID) ;
                     sprintf(_gi[j].FileVarName, "%s", _si[i].FileVarName) ;
                     sprintf(_gi[j].BaseValue, "%s", _si[i].BaseValue) ;
                     sprintf(_gi[j].MoveTo, "%s|%s|%s|%s|%s|%s", tmp_3DC[1].x1, tmp_3DC[1].x2, tmp_var, tmp_3DC[1].y2, tmp_3DC[1].z1, tmp_3DC[1].z2); 
                     tmp_var += step;

                     if ( j++ == MAXFILENUM-1)
                     {
                        printf( " the files to be generated is too many!\n");
                        return -1;
                     }
                 }
             } else if ( tmp_3DC[1].z1 < tmp_3DC[2].z1 ){
                 double step = (tmp_3DC[2].z1 - tmp_3DC[1].z1)/(atoi(_si[i].Divisions)+1);
                 double tmp_var = tmp_3DC[1].z1;

                 while ( tmp_var < tmp_3DC[2].z1 || fabs( tmp_var - tmp_3DC[2].z1) < ZERO )
                 {
                     sprintf(_gi[j].VarType, "%s", _si[i].VarType) ;
                     sprintf(_gi[j].Alias, "%s", _si[i].Alias) ;
                     sprintf(_gi[j].FDS_ID, "%s", _si[i].FDS_ID) ;
                     sprintf(_gi[j].FileVarName, "%s", _si[i].FileVarName) ;
                     sprintf(_gi[j].BaseValue, "%s", _si[i].BaseValue) ;
                     sprintf(_gi[j].MoveTo, "%s|%s|%s|%s|%s|%s", tmp_3DC[1].x1, tmp_3DC[1].x2, tmp_3DC[1].y1, tmp_3DC[1].y2, tmp_var, tmp_3DC[1].z2); 
                     tmp_var += step;

                     if ( j++ ==MAXFILENUM-1 )
                     {
                        printf( " the files to be generated is too many!\n");
                        return -1;
                     }
                 }
             } else if ( tmp_3DC[1].x2 < tmp_3DC[2].x2 ) {
                 double step = (tmp_3DC[2].x2 - tmp_3DC[1].x2)/(atoi(_si[i].Divisions)+1);
                 double tmp_var = tmp_3DC[1].x2;

                 while ( tmp_var < tmp_3DC[2].x2 || fabs( tmp_var - tmp_3DC[2].x2) < ZERO )
                 {
                     sprintf(_gi[j].VarType, "%s", _si[i].VarType) ;
                     sprintf(_gi[j].Alias, "%s", _si[i].Alias) ;
                     sprintf(_gi[j].FDS_ID, "%s", _si[i].FDS_ID) ;
                     sprintf(_gi[j].FileVarName, "%s", _si[i].FileVarName) ;
                     sprintf(_gi[j].BaseValue, "%s", _si[i].BaseValue) ;
                     sprintf(_gi[j].MoveTo, "%s|%s|%s|%s|%s|%s", tmp_3DC[1].x1,tmp_var, tmp_3DC[1].y1, tmp_3DC[1].y2, tmp_3DC[1].z1, tmp_3DC[1].z2); 
                     tmp_var += step;

                     if ( j++ == MAXFILENUM-1 )
                     {
                        printf( " the number of files to be generated is too big!\n");
                        return -1;
                     }
                 }
             } else if ( tmp_3DC[1].y2 < tmp_3DC[2].y2 ){
                 printf( "13:   i=%d, _si[%d].VarType=%s\n", i, i, _si[i].VarType );
                 double step = (tmp_3DC[2].y2 - tmp_3DC[1].y2)/(atoi(_si[i].Divisions)+1);
                 double tmp_var = tmp_3DC[1].y2;

                 while ( tmp_var < tmp_3DC[2].y2 || fabs( tmp_var - tmp_3DC[2].y2) < ZERO )
                 {
                     sprintf(_gi[j].VarType, "%s", _si[i].VarType) ;
                     sprintf(_gi[j].Alias, "%s", _si[i].Alias) ;
                     sprintf(_gi[j].FDS_ID, "%s", _si[i].FDS_ID) ;
                     sprintf(_gi[j].FileVarName, "%s", _si[i].FileVarName) ;
                     sprintf(_gi[j].BaseValue, "%s", _si[i].BaseValue) ;
                     sprintf(_gi[j].MoveTo, "%s|%s|%s|%s|%s|%s", tmp_3DC[1].x1, tmp_3DC[1].x2, tmp_3DC[1].y1,tmp_var, tmp_3DC[1].z1, tmp_3DC[1].z2); 
                     tmp_var += step;

                     if ( j++ == MAXFILENUM-1)
                     {
                        printf( " the number of files to be generated is too big!\n");
                        return -1;
                     }
                 }
             } else if ( tmp_3DC[1].z2 < tmp_3DC[2].z2 ){
                 double step = (tmp_3DC[2].z2 - tmp_3DC[1].z2)/(atoi(_si[i].Divisions)+1);
                 double tmp_var = tmp_3DC[1].z2;

                 while ( tmp_var < tmp_3DC[2].z2 || fabs( tmp_var - tmp_3DC[2].z2) < ZERO )
                 {
                     sprintf(_gi[j].VarType, "%s", _si[i].VarType) ;
                     sprintf(_gi[j].Alias, "%s", _si[i].Alias) ;
                     sprintf(_gi[j].FDS_ID, "%s", _si[i].FDS_ID) ;
                     sprintf(_gi[j].FileVarName, "%s", _si[i].FileVarName) ;
                     sprintf(_gi[j].BaseValue, "%s", _si[i].BaseValue) ;
                     sprintf(_gi[j].MoveTo, "%s|%s|%s|%s|%s|%s", tmp_3DC[1].x1, tmp_3DC[1].x2, tmp_3DC[1].y1, tmp_3DC[1].y2, tmp_3DC[1].z1,tmp_var); 
                     tmp_var += step;

                     if ( j++ ==MAXFILENUM-1 )
                     {
                        printf( " the files to be generated is too many!\n");
                        return -1;
                     }
                 }
             }
          }

          if( _si[i].VarType[2] == 'P' ) // 'P' for Physical (non-geographic) variables
          {
              double step = (atof(_si[i].UpperLimit) - atof(_si[i].LowerLimit))/(atoi(_si[i].Divisions)+1);
              double tmp_var = atof(_si[i].LowerLimit);
/*
              if( _si[i].VarType[1] == 'S' && atoi(_si[i].Divisions) != 0 )
                  step = (atof(_si[i].UpperLimit) - atof(_si[i].LowerLimit));
*/
                 
              //printf( "step = %lf, _si[%d].BaseValue=%lf, _si[%d].LowerLimit=%lf, _si[%d].UpperLimit=%lf\n", step,i,atof(_si[i].BaseValue), i,atof(_si[i].LowerLimit), i, atof(_si[i].UpperLimit));
              if( atof(_si[i].BaseValue) < atof(_si[i].LowerLimit) || atof(_si[i].BaseValue) > atof(_si[i].UpperLimit) )
              {
                 printf( "baseline coordinates %s must be between the LowerLimit %s and UpperLimit %s!\n", 
                          _si[i].BaseValue, _si[i].LowerLimit, _si[i].UpperLimit );
                   return -1;
              }

              while ( tmp_var < atof(_si[i].UpperLimit) || fabs( tmp_var - atof(_si[i].UpperLimit)) < ZERO )
              {
                  sprintf(_gi[j].VarType, "%s", _si[i].VarType) ;
                  sprintf(_gi[j].Alias, "%s", _si[i].Alias) ;
                  sprintf(_gi[j].FDS_ID, "%s", _si[i].FDS_ID) ;
                  sprintf(_gi[j].FileVarName, "%s", _si[i].FileVarName) ;
                  sprintf(_gi[j].BaseValue, "%s", _si[i].BaseValue) ;
                  sprintf(_gi[j].MoveTo, "%.6f", tmp_var );
                  tmp_var += step;

               //   printf( "j=%d, _gi.VarType=%s, _gi.FileVarName=%s, _gi.BaseValue=%s, _gi.MoveTo=%s\n", j, _gi[j].VarType, _gi[j].FileVarName, _gi[j].BaseValue, _gi[j].MoveTo );

                  if ( j++ == MAXFILENUM-1)
                  {
                        printf( " the number of files to be generated is too big!\n");
                        return -1;
                  }
              }
          }

          if( _si[i].VarType[2] == 'C' ) //'C' for combined variables changing at the same time
          {
              sprintf(_gi[j].VarType, "%s", _si[i].VarType) ;
              sprintf(_gi[j].Alias, "%s", _si[i].Alias) ;
              sprintf(_gi[j].FDS_ID, "%s", _si[i].FDS_ID) ;
              sprintf(_gi[j].FileVarName, "%s", _si[i].FileVarName) ;
              sprintf(_gi[j].BaseValue, "%s", _si[i].BaseValue) ;

              if( atoi(_si[i].Divisions) == 1 ) // changes to UpperLimit
                  sprintf(_gi[j].MoveTo, "%s", _si[i].UpperLimit );
              else if( atoi(_si[i].Divisions) == -1 ) // changes to LowerLimit 
                  sprintf(_gi[j].MoveTo, "%s", _si[i].LowerLimit);
              else {
                  printf( "Divisions [%s] illegal for VarType [%s]\n", _si[i].Divisions, _si[i].VarType );
                  return -1;
              }
              if ( j++ == MAXFILENUM-1)
              {
                  printf( " the files to be generated are too much!\n");
                  return -1;
              }
          }

          i++;
     } 

     // print _gi
     for ( j=0;j<MAXFILENUM;j++ )
     {
         if( strlen(trim(_gi[j].VarType,NULL)) == 0 )
             break;
         printf( "%d: VarType=%s, FDS_ID=%s, FileVarName=%s, BaseValue=%s, MoveTo=%s\n",j, _gi[j].VarType, _gi[j].FDS_ID,_gi[j].FileVarName, _gi[j].BaseValue, _gi[j].MoveTo);
     }
    return 0;
}

int main( int argc, char ** argv )
{
    struct SMInfo FDS_SmInfo[MAXFILENUM];
    struct GenInfo FDS_GenInfo[MAXFILENUM];

    if ( argc != 2 )
    {
        int i=0;
        for ( i=0; i<argc; i++ )
           printf( "%s\n", argv[i] );
        printf( "only one argument is needed, you have [%d] arguments\n usage: ./GenFiles SM_Info.txt\n" , argc);
        return -1;
    }

    memset( FDS_SmInfo, '\0', sizeof(FDS_SmInfo));
    memset( FDS_GenInfo, '\0', sizeof(FDS_GenInfo));

    if ( readin(argv[1], FDS_SmInfo) != 0 ){  //readin the input configuration file (SM_Info.txt) into FDS_SmInfo)
        printf( "read SMInfo to structure error!\n" );
        return -1;
    }
    if( GenFiles( FDS_SmInfo, FDS_GenInfo) != 0){ //generate the list of files to be created 
        printf( "GenFiles error!\n" );
        return -1;
    }  
    if( OutputFiles( basefile,FDS_GenInfo ) != 0 ){ //according to the to-do list in FDS_GenInfo, derive FDS new files based on basefile 
        printf( "OutputFiles error!\n" );
        return -1;
    }

    return 0;
}
