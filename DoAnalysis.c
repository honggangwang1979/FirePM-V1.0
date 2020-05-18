
/****************************************************************************************************************************************************
 * Author: Honggang Wang and Professor Dembsey, the Department of Fire Protection Engineering in WPI
 * Copyright: This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation with the author information being cited
 *
 * Discription: 
 *      This file includes the source code of the tool, DoA, which can be used to analyze the simulation results of FDS files created by the other tool, GenFiles. the analysis results are stored into FDS_DoA which includes anything simulated, FDS_RSM which includes the information needed for RSM (response surface method) analysis, FDS_CMB which includes the combined fire scenario results and the comparison between FDS simulation results, SMM(sensitivity matrix method) prediction and the RSM prediction, FDS_SenMat which includes the sensitivity matrix details, FDS_RSMResults which includes the power curve fitting parameters of RSM, and SenMatx which is a 2D string array including only the sensitivity matrix. 
 *
 * How to Run this tool: ./DoA SM_Info.txt
 *
 * Flowchat: 
 *     step 1 -> read information from user's input file (SM_Info.txt) into  a SMInfo struct array (FDS_SmInfo)
 *     step 2 -> extract useful information from the FDS simulation result files (*devc.csv or *evac.csv ) and calcuate these informaiton and save to _DoA
 *     step 3 -> calculate the sensitivity matrix from _DoA and _si and save the result to _SMT
 *     step 4 -> calculate the power curve fitting parameters from _DoA and _si and save the result to RSM/RSMRlt; compare the FDS simulation result with the SMM prediction and RSM prediction and save the result to CMB
 ***************************************************************************************************************************************************/

#include "FirePM.h"

struct ThreeDCoordinate ThreeDC[3]; //to store the 3d coordinates;
struct SMInfo FDS_SmInfo[MAXLINENUM]; //to store the input initial configuration  file (SM_Info.txt in this case )
struct DoAlxInfo FDS_DoA[MAXLINENUM]; //to store the calculated the outputnewvalue based on the configuration file information
struct DoAlxInfo FDS_RSM[MAXLINENUM]; //to store information refined from the FDS_DoA for Response Surface Method (RSM)
struct DoAlxInfo FDS_CMB[MAXLINENUM]; //to store information refined from the FDS_DoA for Combined inputs(RSM) used to verify/update the RSM and SMT
struct SenMat FDS_SenMat[MAXLINENUM]; //to store sensitivity matrix information refined from the FDS_DoA and FDS_SmInfo 
struct RSMResults FDS_RSMResults[(MAXINPUTSNUM+1)*(MAXOUTPUTSNUM+1)]; //to store the results of Response Surface Method


/************************************************************************************************************************************************** 
 * Functions: Obtain all the directories that may have the FDS simulation results
 * _si: input parameter holding the information of the configuration file (SM_Info.txt in our case)
 * _dl: output parameter holding the found directories 
 * returns: 0 : success; -1 : fail
 *************************************************************************************************************************************************/
int GetDirList( struct SMInfo *_si, char _dl[][100] )
{
    int i=0;
    char tmp_dir_name[200];
 
    memset( tmp_dir_name, 0x0, sizeof(tmp_dir_name));

    if ( getcwd(tmp_dir_name, sizeof(tmp_dir_name)) == NULL ) //get current working directory
    {
       perror("getcwd() error");
       return -1;
    }

    //get all the directories that may have the FDS simulation results
    for(i=0;i<MAXLINENUM;i++)
    {
       int flag = 0; // indicating if a directory has already been included into the directory list
       int tmp_j=0;
      
       if( strlen(_si[i].VarType) == 0 || _si[i].VarType[0] == 'O')
           break;

       //printf( "i=%d, tmp_j=%d\n", i, tmp_j );
       for( tmp_j=0; tmp_j<MAXNEWDIRNUM; tmp_j++ )
       {
           //printf( "i=%d, tmp_j=%d, _dl=%s\n", i, tmp_j, _dl[tmp_j] );
           if( strlen(_dl[tmp_j]) == 0 ) // the end of the DirList, break 
               break;
           if( strstr(_dl[tmp_j], _si[i].VarType) != NULL )
           {
              flag = 1;  // already in the DirList
              break;
           }
       }

       if( flag  != 1 ) // cannot find _si[i].VarType, then add into DIRList, at this time tmp_j is the end of the list
       {
          sprintf(_dl[tmp_j], "%s/%s", tmp_dir_name, _si[i].VarType); 
          printf( "i=%d, tmp_j=%d, _dl=%s\n", i, tmp_j, _dl[tmp_j] );
       }
    }

    //print the DirList information
    for( i=0; i<MAXNEWDIRNUM; i++ )
    {
        if( strlen(_dl[i]) != 0 )
           printf("DirList[%d] is =[%s]\n", i, _dl[i] );
        else
           break;
    }

    return 0;
}

/* ************************************************************************************************************************************************
 * Functions: check if a file is what we need because we are only dealing with devc or evac csv files
 * _fn: input parameter indication a fire name
 * Return: 0: Yes, this file may be useful, -1: No, we don't need this file
 *************************************************************************************************************************************************/
int FileFilter(char *_fn)
{
    if( strstr( _fn, ".csv") == NULL )//not a csv file
        return -1;

    if( (strstr( _fn, "devc") == NULL) && (strstr(_fn, "evac")==NULL)) // is a csv file but not a devc or evac file
        return -1;

    return 0;
}

/************************************************************************************************************************************************** 
 * Functions: print one DoA record to a file indicated by _fp
 * _D: input parameter indicating one single DoA record
 * _fp: input parameter indicating a fire pointer
 * Return: Void 
 **************************************************************************************************************************************************/
void PrintOneDoA( struct DoAlxInfo _D, FILE *_fp )
{
     int i=0;

     for( i=0; i<MAXOUTPUTSNUM; i++ )
     {
         char tmp_str[MAXSTRINGSIZE];

         if( strlen(_D.OutputVarType[i]) == 0 )
             return;

         memset(tmp_str, 0x0, sizeof(tmp_str));
         sprintf( tmp_str, "%s, %s, %s, %s, %s, %s, %s, %s, %.2f, %s, %.2f\n", _D.InputVarType, _D.OutputVarType[i], _D.InputAlias, _D.OutputAlias[i],_D.TargetName[i], _D.InputFileVarName, _D.OutputFileVarName[i], _D.InputBaseValue, _D.OutputBaseValue[i], _D.InputNewValue, _D.OutputNewValue[i] ); 
         fputs( tmp_str, _fp );
         printf( "_D.flag=%d,_D.OutputVarType[%d]=%s, _D.OutputAlias[%d]=%s,_D.OutputFileVarName[%d]=%s,_D.OutputBaseValue[%d]=%lf,_D.OutputNewValue[%d]=%lf \n",_D.flag, i, _D.OutputVarType[i], i,_D.OutputAlias[i], i,_D.OutputFileVarName[i], i,_D.OutputBaseValue[i], i, _D.OutputNewValue[i]);
     }
} 

/************************************************************************************************************************************************ 
 * Functions: use the information from _single_si to find the input base value and store it to _Ibv
 * _Ibv: output parameter indicating Inputbasevalue
 * _single_si: input parameter indicating one single SMInfo record
 * Return: 0 : success, -1: failure 
 ***********************************************************************************************************************************************/
int getInputBaseValue( char *_Ibv, struct SMInfo _single_si )
{
    if( strstr(_single_si.BaseValue, "|" ) != NULL ) // indicates that the input variabel is geographic variable
    {
        int i=0;
        double tmp_dc_0[6], tmp_dc_1[6], tmp_dc_2[6];

        memset( tmp_dc_0, 0x0, sizeof(tmp_dc_0));
        memset( tmp_dc_1, 0x0, sizeof(tmp_dc_1));
        memset( tmp_dc_2, 0x0, sizeof(tmp_dc_2));
        
        // change the basevalue , lowerlimit , upperlimit to 3 coordinates structures 
        sscanf(_single_si.BaseValue,"%lf|%lf|%lf|%lf|%lf|%lf",
                &(tmp_dc_0[0]), &(tmp_dc_0[1]), &(tmp_dc_0[2]), &(tmp_dc_0[3]), &(tmp_dc_0[4]), &(tmp_dc_0[5]) );
        sscanf(_single_si.LowerLimit,"%lf|%lf|%lf|%lf|%lf|%lf",
                &(tmp_dc_1[0]), &(tmp_dc_1[1]), &(tmp_dc_1[2]), &(tmp_dc_1[3]), &(tmp_dc_1[4]), &(tmp_dc_1[5]) );
        sscanf(_single_si.UpperLimit,"%lf|%lf|%lf|%lf|%lf|%lf",
                &(tmp_dc_2[0]), &(tmp_dc_2[1]), &(tmp_dc_2[2]), &(tmp_dc_2[3]), &(tmp_dc_2[4]), &(tmp_dc_2[5]) );

        for( i=0;i<6;i++)
        {
            if ( (fabs(tmp_dc_0[i]-tmp_dc_1[i]) > ZERO) || (fabs(tmp_dc_0[i]-tmp_dc_2[i]) > ZERO ) )
            {
               if( i%2 == 0 ) //x1,y1,z1
               //width = x1-x2 or y1-y2  or z1-z2, because each (and only one) coordinate can be variable
                   sprintf( _Ibv, "%lf", fabs(tmp_dc_0[i]-tmp_dc_0[i+1]) ); 
               else  //x2,y2,z2
               //width = x2-x1 or y2-y1  or z2-z1, because each (and only one) coordinate can be variable
                   sprintf( _Ibv, "%lf", fabs(tmp_dc_0[i]-tmp_dc_0[i-1]) ); 
                return 0;
            }
        }
    } else { // indicates that the input variabel is physical variable, copy it directly into _Ibv
        sprintf( _Ibv, "%s", _single_si.BaseValue );
        return 0;
    }
    return -1;
}

/************************************************************************************************************************************************
 * Functions: fill the DoA list by using the csv files in _dir which could mach the input and output records in  _si. *P indicates the tail
 *            of the DoA list. 
 *            flow chat:
 *            for each file in the _dir (while{})
 *            1. check if we need this file ( CheckCSV() ). if the answer is yes, keep going, otherwise go to check next file
 *            2. for each record in configuration file (_si), check if it matches the current file. if yes, keep going, otherwise go to next line
 *            3. collect the input information and put them into the _DoA list at position of *_p (getInputBasevalue(), getInputNewValue())
 *            4. collect the output information and put them into the _DoA list at [position of *_p(getOutputValue());
 *            5. move to next position (*_p++)
 *_Dir: input parameter indicating the directory including the *devc.csv or *evac.csv files
 *_si: input parameter indicating input and output variables' information by which the csv files in _dir are checked
 *_DoA: output parameter inidcating the analysis results from _si and the .csv files in _dir
 *_p: output parameter indicating the end position of the structure _DoA so that the next addition can becan from this point
 *Return: 0: success, -1: failure
 *********************************************************************************************************************************************/
int FillDoA( char * _dir, struct SMInfo * _si, struct DoAlxInfo *_DoA, int *_p)
{
    DIR *tmp_dir = NULL;
    struct dirent *en = NULL;

    char Info[MAXSTRINGSIZE];

    //Although _DoA is defined with quite a large array size (MAXLINENUM), we need to check  if the limitation will be met 
    if( *_p == MAXLINENUM )
    {
        printf( "the size limitation of _DoA is met: _p=%d\n", *_p );
        return -1;
    }

    //if the curent record referred by *_P is not empty, return fail becasue *_p should indicate a new record at the end of the struct list
    if( strlen(_DoA[*_p].InputVarType) !=  0 )
    {
        printf( "_DoA[%d] is not empty, will overwrite,  InputVarType = [%s]\n", *_p, _DoA[*_p].InputVarType );
        return -1;
    }

    // clean the buff of Info
    memset( Info, '\0', sizeof(Info));

    tmp_dir = opendir(_dir); //open the directory
    if (tmp_dir == NULL)
    {
        printf( "opendir error: %s\n", _dir );
        perror("Error:");
        return -1;
    }

    // for each file in the _dir, seek the useful file (*devc.csv or *evac.csv) and use the informaiton from _si to fill the _DoA 
    while ((en = readdir(tmp_dir)) != NULL)
    {
        FILE *fp = NULL;
        FILE *tmp_fp = NULL;
        char tmp_fn[MAXSTRINGSIZE];
        char tmp_whole_fn[MAXSTRINGSIZE];
        int tmp_i=0;

        //clean the buffers 
        memset( tmp_fn, 0x0, sizeof(tmp_fn));
        memset( tmp_whole_fn, 0x0, sizeof(tmp_whole_fn));

        //compose the whole file name
        sprintf( tmp_whole_fn, "%s/%s", _dir, en->d_name );
        if( CheckCSV(tmp_whole_fn, _si ) != 0 ) // to make sure this file is what we need 
        {
            // printf( "file [%s] doesn't match!\n", tmp_whole_fn );
            continue;
        }

        printf( "tmp_whole_fn=%s\n", tmp_whole_fn); //print all directory name

        // this for sentense builds one or many _DoA elements based on the file (tmp_whole_fn) and the _si[tmp_i]
        for( tmp_i = 0; tmp_i < MAXLINENUM; tmp_i++ ) 
        {
            int rt = 0;
            if( strlen(_si[tmp_i].VarType) == 0 )
                break;

            //printf( "in for 1. si[%d].VarType=%s\n", tmp_i, _si[tmp_i].VarType );

            //Although in CheckCSV(), IsFileMatch() is called to make sure tmp_whole_fn is what we needed, here IsFileMatch() needs to be called again to make sure the single struct _si[tmp_i] can match the file of tmp_whole fn
            if( IsFileMatch(tmp_whole_fn, _si[tmp_i] ) != 0)
                continue;

            //printf( "in for 2. si[%d].VarType=%s, Alias=%s, FileVarName=%s, LowerLimit=%s, UpperLimit=%s\n", tmp_i, _si[tmp_i].VarType, _si[tmp_i].Alias, _si[tmp_i].FileVarName,_si[tmp_i].LowerLimit, _si[tmp_i].UpperLimit);

            // copy information from _si[tmp_i] to _DoA[*_p]
            sprintf( _DoA[*_p].InputVarType, "%s", _si[tmp_i].VarType ); 
            sprintf( _DoA[*_p].InputAlias, "%s", _si[tmp_i].Alias); 
            sprintf( _DoA[*_p].InputFileVarName, "%s", _si[tmp_i].FileVarName); 
            sprintf( _DoA[*_p].comment[0],"LowerLimit=[%s]   UpperLimit=[%s]", _si[tmp_i].LowerLimit, _si[tmp_i].UpperLimit);

            //printf( "in for 3. *_p = %d, si[%d].VarType=%s, Alias=%s, FileVarName=%s, LowerLimit=%s, UpperLimit=%s\n", *_p, tmp_i, _si[tmp_i].VarType, _si[tmp_i].Alias, _si[tmp_i].FileVarName,_si[tmp_i].LowerLimit, _si[tmp_i].UpperLimit);
            // find inputBaseValue from _si[tmp_i]
            if( getInputBaseValue( _DoA[*_p].InputBaseValue, _si[tmp_i]) != 0 )
            {
                printf( "getInputBaseValue() error: *_p = [%d], tmp_i = [%d]\n", *_p, tmp_i );
                return -1;
            }
            //printf( "in for 4. *_p = %d, si[%d].VarType=%s, Alias=%s, FileVarName=%s, LowerLimit=%s, UpperLimit=%s\n", *_p, tmp_i, _si[tmp_i].VarType, _si[tmp_i].Alias, _si[tmp_i].FileVarName,_si[tmp_i].LowerLimit, _si[tmp_i].UpperLimit);
            
            if( (rt=getInputNewValue( _DoA[*_p].InputNewValue, tmp_whole_fn, _si[tmp_i])) == -1 )
            {
                printf( "getInputNewValue() error, _DoA[%d], tmp_whole_fn=[%s], _si[%d].FileVarName=[%s]\n", *_p, tmp_whole_fn,tmp_i, _si[tmp_i].FileVarName );
                return -1;
            } else if ( rt == 1 ) //basevalue is same to InputNewValue
            {
                sprintf( _DoA[*_p].InputNewValue, "%s", _si[tmp_i].BaseValue); 
            }
            
            //printf( "in for 5. *_p = %d, si[%d].VarType=%s, Alias=%s, FileVarName=%s, LowerLimit=%s, UpperLimit=%s\n", *_p, tmp_i, _si[tmp_i].VarType, _si[tmp_i].Alias, _si[tmp_i].FileVarName,_si[tmp_i].LowerLimit, _si[tmp_i].UpperLimit);
            if( getOutputValues( _DoA, *_p, tmp_whole_fn, _si ) != 0 )
            {
                printf( "getOutputValue() error, _DoA[%d], tmp_whole_fn=[%s], _si[%d].FileVarName=[%s]\n", *_p, tmp_whole_fn, tmp_i, _si[tmp_i].FileVarName );
                return -1;
            }
            //printf( "after getOutputValues: _DoA[%d].OutputNewValue[0]=[%lf],_si[%d].VarType=%s\n", *_p, _DoA[*_p].OutputNewValue[0],tmp_i, _si[tmp_i].VarType );
            (*_p)++;
        }

     }
} 

/***************************************************************************************************************************************************
 * Function: fill _DoA with output data in file _fn which match _si's output records.
 *           Flow chat: for each record in the configuration file,
 *           1. check if the record is output record, if Yes, keep going, otherwise do nothing and go to next record
 *           2. get the second line of the csv file (_fn)
 *           3. find the column position of the output variable of FDS on which the critical value is put(getColumnPos())
 *           4. find the column position of the output variable of FDS which is the target corresponding to the critical value
 *           5. find the output base value of the target variable which should be in the FDS simulation data file of the baseline case
 *           6. find two pairs of values of critical variable and target variable which are neighbor
 *           7. if the last critical variable met the critical value, then calculate the value of the target variable by interpolation
 *           8. if the last critical variable finally cannot be met, print to the screen this information and set the flog of _DoA to 1 so that this                 record will not be processed later on. This happens when the simulation time is not enough or the FDS file is not set up well.
 *_DoA: Output parameter indicating a struct list of analysis data
 *_index: Input parameter indicating the position at _DoA needing information 
 *_si: Input parameter indicating the content in the configuration file, only the output part of the file will be used in this function.
 *Return: 0: success, -1: failure
 **************************************************************************************************************************************************/
int getOutputValues( struct DoAlxInfo *_DoA, int _index, char * _fn, const struct SMInfo *_si )
{
    FILE *fp = NULL;
    int i=0,k=0;
    char buff_1[MAXSTRINGSIZE];
    char buff_2[MAXSTRINGSIZE];

    memset( buff_1, 0x0, sizeof(buff_1));
    memset( buff_2, 0x0, sizeof(buff_2));
    
    fp=fopen(_fn,"r");
    if( fp == NULL )
    {
        printf( "open file [%s]failed!\n",_fn);
        return -1;
    }

    printf( "before for in getOutputValues: \n" );

    for( i=0; i<MAXLINENUM; i++ )
    {
        if( _si[i].VarType[0] == 'O' ) //only seek output records
        {
           fseek(fp, 0, SEEK_SET);
           fgets(buff_1,sizeof(buff_1),fp); // first line of csv file is not used
           if( fgets(buff_1, sizeof(buff_1), fp ) != NULL )
           {
               double tmp_first_value=0.00;
               double tmp_basevalue = 0.00;
               double tmp_first_NV= 0.00;
               double tmp_first_CV = atof(_si[i].CriticalValue);
               int tmp_first_direction = atoi(_si[i].Divisions);
               //column indicates the column number of the output critical variable, and column_TN means column number of the target variable
               int column,column_TN;  
               int flag=0; // check if the critical value is met in the file fp

               //printf( "in for 1 in  getOutputValues(): buff_1 = %s, _si[%d].FileVarName=%s, s=%s\n", buff_1, i,_si[i].FileVarName, s );
               //find the column number of the output critical variable in the csv file
               column = getColumnPos(buff_1, _si[i].FileVarName,s);
               if( column == -1 )
               {
                   printf("output variable [%s] not found in [%s]!\n", _si[i].FileVarName, buff_1 );
                   fclose(fp);
                   return -1;
               }

               //printf( "in for 2 in  getOutputValues(): buff_1 = %s, _si[%d].FileVarName=%s, s=%s, column=%d\n", buff_1, i,_si[i].FileVarName, s, column );
               //find the column number of the output target variable in the csv file
               column_TN = getColumnPos(buff_1, _si[i].TargetName,s);
               if( column_TN == -1 )
               {
                   printf("output variable [%s] not found in [%s]!\n", _si[i].TargetName, buff_1 );
                   fclose(fp);
                   return -1;
               }

               //printf( "in for 3 in  getOutputValues(): buff_1 = %s, _si[%d].TargetName=%s, s=%s, column_TN=%d\n", buff_1, i,_si[i].TargetName, s, column_TN );

               //basefile is a global variable holding the baseline FDS file name which is assigned when reading in the configuration file
               if( getOutputBaseValue(basefile, _si[i], &tmp_basevalue) == -1 ) 
               {
                   printf( "getOutputBaseValue() error: basefile=[%s], _si[%d], column=[%d]!\n", basefile, i, column);
                   fclose( fp );
                   return -1;
               }
               //printf( "in for 4 in  getOutputValues(): buff_1 = %s, _si[%d].TargetName=%s, s=%s, column_TN=%d\n", buff_1, i,_si[i].TargetName, s, column_TN );

               if( column == -1 )
               {
                   printf("output variable [%s] not found in [%s]!\n", _si[i].FileVarName, buff_1 );
                   fclose( fp );
                   return -1;
               }
               fgets(buff_1,sizeof(buff_1),fp); // get the first data line in the file _fn

               //find the value at the column and put it into tmp_first_value
               if( getValueByCol(buff_1, column,s,&tmp_first_value) != 0 )
               {
                   printf("getValueByCol() error, buff_1 = [%s], column = [%d], sep=[%s]!\n", buff_1, column, s );
                   fclose( fp );
                   return -1;
               }

               //printf( "in for 5 in  getOutputValues(): buff_1 = %s, _si[%d].TargetName=%s, s=%s, column_TN=%d\n", buff_1, i,_si[i].TargetName, s, column_TN );
               //find the value at the column and put it into tmp_first_NV. 
               if( getValueByCol(buff_1, column_TN, s, &tmp_first_NV) != 0 )
               {
                   printf("getValueByCol() error, buff_1 = [%s], column = [1], sep=[%s]!\n", buff_1, s );
                   fclose( fp );
                   return -1;
               }
               printf( "in for 6 in  getOutputValues(): buff_1 = %s, _si[%d].TargetName=%s, s=%s, column_TN=%d\n", buff_1, i,_si[i].TargetName, s, column_TN );

               // the first line may be what we need, although the possibility is very very very very very low
               if( (tmp_first_direction == 1 && tmp_first_value > tmp_first_CV )|| ( tmp_first_direction == -1 && tmp_first_value < tmp_first_CV ))
               {
                   double tmp_d=0.0;
               	   if( getValueByCol(buff_1, column_TN, s, &tmp_d) != 0 )
                   {
                       printf("getValueByCol() error, buff_1 = [%s], column_TN = [%d], sep=[%s]!\n", buff_1, column_TN, s );
                       fclose( fp );
                       return -1;
                   }
                   _DoA[_index].OutputNewValue[k] = tmp_d;
                   _DoA[_index].OutputBaseValue[k] = tmp_basevalue; 
                   sprintf( _DoA[_index].OutputVarType[k],"%s", _si[i].VarType );
                   sprintf( _DoA[_index].OutputAlias[k],"%s", _si[i].Alias);
                   sprintf( _DoA[_index].TargetName[k],"%s", _si[i].TargetName);
                   sprintf( _DoA[_index].OutputFileVarName[k],"%s", _si[i].FileVarName);
                   sprintf( _DoA[_index].OutputFileVarName[k],"%s", _si[i].FileVarName);

                   if( k> 0 )
                       sprintf( _DoA[_index].comment[k], "%s", _DoA[_index].comment[0]);
                   k++; //there may be more than one output variables, k indicates the position 
                   continue;
               }
 
               do 
               {
                  int tmp_direction = tmp_first_direction;
                  double tmp_value_1 = tmp_first_value;
                  double tmp_value_2 = 0.0;
                  double tmp_CV= tmp_first_CV;
                  double tmp_NV_1=tmp_first_NV; 
                  double tmp_NV_2=0.0; 
                 
                  fgets(buff_2,sizeof(buff_2),fp);
               	  if( getValueByCol(buff_2, column, s, &tmp_value_2) != 0 ) //find the second pair of data
                  {
                      printf("getValueByCol() error, buff_1 = [%s], column = [%d], sep=[%s]!\n", buff_2, column, s );
                      fclose( fp );
                      return -1;
                  }
               	  if( getValueByCol(buff_2, column_TN, s, &tmp_NV_2) != 0 ) //find the second pair of data
                  {
                      printf("getValueByCol() error, buff_1 = [%s], column_TN = [%d], sep=[%s]!\n", buff_2, column_TN, s );
                      fclose( fp );
                      return -1;
                  }

                  if((tmp_direction == 1 && tmp_value_2 > tmp_CV )||
                     ( tmp_direction == -1 && tmp_value_2 < tmp_CV ))
                  { //the critical value is met, call the interpolation function CalMidVal()
                     _DoA[_index].OutputNewValue[k] = CalMidVal(tmp_NV_1, tmp_NV_2, tmp_value_1, tmp_value_2, tmp_CV); 
printf( "_DoA[%d].OutputNewValue[%d]=[%lf],tmp_NV_1=[%lf],tmp_NV_2=[%lf],tmp_value_1=[%lf], tmp_Value_2=[%lf],  tmp_CV=[%lf]\n", _index, k, _DoA[_index].OutputNewValue[k], tmp_NV_1, tmp_NV_2, tmp_value_1, tmp_value_2, tmp_CV);
                     _DoA[_index].OutputBaseValue[k] = tmp_basevalue;
                     sprintf( _DoA[_index].OutputVarType[k],"%s", _si[i].VarType );
                     sprintf( _DoA[_index].OutputAlias[k],"%s", _si[i].Alias);
                     sprintf( _DoA[_index].TargetName[k],"%s", _si[i].TargetName);
                     sprintf( _DoA[_index].OutputFileVarName[k],"%s", _si[i].FileVarName);
                     sprintf( _DoA[_index].OutputFileVarName[k],"%s", _si[i].FileVarName);
                     if( k> 0 )
                         sprintf( _DoA[_index].comment[k], "%s", _DoA[_index].comment[0]);
                     flag = 1;
                     k++;
                     break;
                  }
                  tmp_first_value = tmp_value_2; //marching the data pair ahead by updating the first value with the second one
                  tmp_first_NV= tmp_NV_2; //marching the data pair ahead by updating the first value with the second one
    	       }while((getc(fp))!=EOF); //stop at EOF


               if( flag == 0 ) // the critical value is not met, maybe the simulation time is not enough, check the csv file
               {
                   printf( "critical value [%s] cannot be met in csv [%s] !\n", _si[i].CriticalValue, _fn);
                   _DoA[_index].flag = 1; // the simulationg time may be too short to meet the critical value, or the critical value is too small or too big to be met, in thiese cases set the DoA.flag to 1 indicates that this record will not be processed later on.
                   //return -1;
               }
           }
        } 
    }
    fclose( fp );
    return 0;
}

/************************************************************************************************************************************************
 * Functions:  calcualte the input new value by using the information from _single_si and _fn and store the input new value to _Inv.
 *   1. Judge if the input variable is a geographic variable or a physical variable
 *   2. if the variable is a geographic one, namely the value includes "|" which seperates the six coordinates of an obstruction/element, then judge  *      if the variable is a combined type (VarType[2]=='C'), if the answer is yes,  break down the coordinate strings with "|" to structures and fin *      d which coordinate is the variable and calculate the changed value and save the value to _Inv
 *   3. if the variable is a physical one, namely the value doesn't include "|", the process is much simple: 1)if the variable is a combined type, th *      en just copy the lowerlimit or upperlimit value to _Inv  based on the value of Divisions 2) if the variable is not a combined type, the file  *      name _fn should have include the input new value, we separate out the value and put it into _Inv
 *_Inv: Output parameter holding the calculated input new value
 *_fn: input parameter indicating a csv file name 
 *_single_si: input parameters indicating one input record listed in the configuration file
 *Return: 0: success, -1 : failure 
 ***********************************************************************************************************************************************/
int getInputNewValue( char * _Inv, const char *_fn, const struct SMInfo _single_si )
{
    int i=0;
    char *tmp_head=NULL;
    char *tmp_tail=NULL;
    char tmp_middle[MAXSTRINGSIZE];
    char tmp_fn[MAXSTRINGSIZE];

    memset( ThreeDC, 0x0, sizeof(ThreeDC) );
    memset( tmp_middle, 0x0, sizeof(tmp_middle) );
    memset( tmp_fn, 0x0, sizeof(tmp_fn) );

    sprintf( tmp_fn, "%s", _fn );

    //if( _single_si.VarType[2] == 'G' || (_single_si.VarType[2]=='C' && strstr(_single_si.BaseValue,"|")!=NULL) )
    if( strstr(_single_si.BaseValue,"|")!=NULL ) // Geographic variables
    {
        //double tmp_dc_0[6],tmp_dc_1[6];
        double tmp_1dc=0.00;
        //memset( tmp_dc_0, 0x0, sizeof(tmp_dc_0) );
        //memset( tmp_dc_1, 0x0, sizeof(tmp_dc_1) );
        

        // for combined variables, a group of records in the configuration files having one same VarType work as one file scenairo 
        if( _single_si.VarType[2] == 'C' ) 
        {
            sscanf(_single_si.BaseValue,"%lf|%lf|%lf|%lf|%lf|%lf",
                &(ThreeDC[1].x1), &(ThreeDC[1].x2), &(ThreeDC[1].y1), &(ThreeDC[1].y2), &(ThreeDC[1].z1), &(ThreeDC[1].z2));
           if( atoi(_single_si.Divisions) == -1 )
           {
                sscanf(_single_si.LowerLimit,"%lf|%lf|%lf|%lf|%lf|%lf",
                    &(ThreeDC[0].x1), &(ThreeDC[0].x2), &(ThreeDC[0].y1), &(ThreeDC[0].y2), &(ThreeDC[0].z1), &(ThreeDC[0].z2));
                if( FindDiffDC(ThreeDC[1],ThreeDC[0], &tmp_1dc) == -1 ) // ThreeDC[1] and tmp3DC[0] are same
                    return 1;
                else
                   sprintf( _Inv, "%lf", tmp_1dc );
              printf( "_Inv = [%s], si.BaseValue=[%s], si.LowerLimit[%s]\n", _Inv,  _single_si.BaseValue, _single_si.LowerLimit );
           }else{
                sscanf(_single_si.UpperLimit,"%lf|%lf|%lf|%lf|%lf|%lf",
                    &(ThreeDC[2].x1), &(ThreeDC[2].x2), &(ThreeDC[2].y1), &(ThreeDC[2].y2), &(ThreeDC[2].z1), &(ThreeDC[2].z2));
                if( FindDiffDC(ThreeDC[1],ThreeDC[2], &tmp_1dc) == -1 ) // ThreeDC[1] and tmp3DC[0] are same
                    return 1;
                else
                   sprintf( _Inv, "%lf", tmp_1dc );
               printf( "_Inv = [%s], si.BaseValue=[%s], si.UpperLimit[%s]\n", _Inv,  _single_si.BaseValue, _single_si.UpperLimit );
           }
          
        } else { //_single_si.VarType[2] == 'G' , including both ISG and IRG of VarType

            int tmp_k=0;
            double tmp_6D_1[6], tmp_6D_2[6];

            memset(tmp_6D_1, 0x0, sizeof(tmp_6D_1) );
            memset(tmp_6D_2, 0x0, sizeof(tmp_6D_2) );

            tmp_head = strstr(tmp_fn, _single_si.Alias);
            if( tmp_head == NULL )
            {
                printf( "tmp_fn [%s] doesn't include [%s]\n", _single_si.Alias );
                return -1;
            }
            tmp_head += strlen(_single_si.Alias)+1;  // seperate out useful information from the filename tmp_fn

            if( get1Data(tmp_head, "_", &tmp_1dc) == -1 ) //tmp_1dc includes the changed coordinate value
            {
                printf( "get1Data() error, tmp_head=%s, sep=[_]\n", tmp_head );
                return -1;
            }
            if( get6Data(_single_si.LowerLimit,"|", tmp_6D_1 ) == -1 ) // break down LowerLimit to a coordinate structure 
            {
                printf( "get6Data() error, si.LowerLimit=%s, sep=[|]\n", _single_si.LowerLimit);
                return -1;
            }
            if( get6Data(_single_si.UpperLimit,"|", tmp_6D_2 ) == -1 ) // break down UpperLimit to a coordinate structure 
            {
                printf( "get6Data() error, si.LowerLimit=%s, sep=[|]\n", _single_si.UpperLimit );
                return -1;
            }

            for( tmp_k = 0; tmp_k<6; tmp_k++ )
            {
// for VarType[1] = 'S' or 'R', if the coordinates at tmp_k are different, the coordinates at other positions are identical since we only support one coordinates as the variable.
                if( fabs( tmp_6D_1[tmp_k] - tmp_6D_2[tmp_k] ) > ZERO ) 
                {
                    if( tmp_k % 2 == 0 ) //x1,y1,z1
                        tmp_1dc = fabs(tmp_1dc - tmp_6D_1[tmp_k+1] );
                    else  //x2,y2,z2
                        tmp_1dc = fabs(tmp_1dc - tmp_6D_1[tmp_k-1] );
                    break; 
                }
            }
            sprintf( _Inv, "%lf", tmp_1dc );
        }

    } else { // physical variables
        char *tmp_inv=NULL;
        char tmp_str[MAXSTRINGSIZE];
        int i=0;

        if( _single_si.VarType[2] == 'C' ) //physical variables in combined fire scenaario 
        {
           if( atoi(_single_si.Divisions) == -1 )
           {
                sprintf( _Inv, "%s", _single_si.LowerLimit);
                printf( "si.VarType =[%s],_Inv = [%s], si.BaseValue=[%s], si.LowerLimit[%s]\n",_single_si.VarType, _Inv,  _single_si.BaseValue, _single_si.LowerLimit );
           }else{
                sprintf( _Inv, "%s", _single_si.UpperLimit);
                printf( "si.VarType = [%s], _Inv = [%s], si.BaseValue=[%s], si.LowerLimit[%s]\n",_single_si.VarType, _Inv,  _single_si.BaseValue, _single_si.UpperLimit );
           } 
               
        } else { //physical variables in RSM or SMM analysis 
            double tmp_1dc=0.0;

            memset( tmp_str, 0x0, sizeof( tmp_str) );
            sprintf( tmp_str, "%s", tmp_fn );

            tmp_head = strstr(tmp_str, _single_si.Alias); //seperate out useful informaiton 
            if( tmp_head == NULL )
            {
                printf( "tmp_head is NULL: tmp_str=%s, Alias=%s\n", tmp_str, _single_si.Alias );
                return -1;
            }
            tmp_head += strlen(_single_si.Alias)+1; 
            if ( get1Data(tmp_head, "_", &tmp_1dc ) == -1 )
            {
                printf( "get1Data() error, tmp_head=%s, sep=[_]\n", tmp_head );
                return -1;
            }
            sprintf( _Inv, "%lf", tmp_1dc);
        }
    }

    return 0;
}

/******************************************************************************************************************************************
 * Functions: check if the filename include the specific VarType and Alias. when the files are generated, the whole file names should include 
 *          the information of VarType and their input Alias
 *_fn: input parameter indicating a file name
 *_single_si: input parameter indicating one single SMInfo struct (one single record in the configuration file SM_Info.txt)
 * Return: 0: if the file matchs the vartype and alias provided by _single_si
 *         -1: the file doesn't match the vartype and alias provided by _single_si
 *******************************************************************************************************************************************/
int IsFileMatch( char *_fn, struct SMInfo _single_si )
{
    if( strstr(_fn, _single_si.VarType) == NULL )
    {
        //printf( "filename [%s] doesn't include [%s]\n", _fn, _single_si.VarType );
        return -1;
    }
    if( strstr(_fn, _single_si.Alias) == NULL )
    {
        //printf( "filename [%s] doesn't include [%s]\n", _fn, _single_si.Alias);
        return -1;
    }

    return 0;
}

/******************************************************************************************************************************************
 * Functions: print the _DoA list to a file (DoA.csv)
 *_DoA_fn:  input parameter indicating the name of the file which will hold the data
 *_DoA: input parameter indicating the calculated analysis data including SMT, RSM and CMB (without refinement)
 * Return: 0: save the file successfully 
 *         -1: save the file error
 *******************************************************************************************************************************************/

int PrintAllDoA( struct DoAlxInfo *_DoA , char * _DoA_fn)
{
     int i = 0;
     char *tmp_str = "InputVarType, OutputVarType, InputAlias, OutputAlias,TargetName, InputFileVarName, OutputFileVarName, InputBaseValue, OutputBaseValue, InputNewValue, OutputNewValue\n";
     FILE *fp=fopen(_DoA_fn, "w+" );
    
     if( fp == NULL )
     {
         printf( "PrintAllDoA() error: cannnot open [%s]\n", _DoA_fn );    
         return -1;
     }
     fputs( tmp_str, fp );
     for( i=0;i<MAXLINENUM;i++)
     {
         if( strlen(_DoA[i].InputVarType) == 0 )
             break;
         PrintOneDoA(_DoA[i], fp);
     }
     fclose( fp );
     return;
}

/******************************************************************************************************************************************
 * Functions: the envelop function of creating the data analysis result which calls the major function of FillDoA() 
 *_DoA: Output parameter indicating the calculated analysis data including SMT, RSM and CMB (without refinement). if _DoA.flag=1, it means the FDs simulation time maybe too short for the critical value to be met.
 *_si:  input parameter including the content of the input configuraiton file
 * Return: 0:sucess
 *         -1: failure
 *******************************************************************************************************************************************/
int GenDoA( struct SMInfo *_si, struct DoAlxInfo *_DoA )
{
    int i=0;
    int DoALength=0; //always indicate the end of the _DoA list
    char DirList[MAXNEWDIRNUM][100];

    memset(DirList, 0x0, sizeof(DirList) );
 

    if( GetDirList(_si, DirList) != 0)
    {
        printf( "GetDirList error!\n" );
        return -1;
    }
    
    printf( "DoALength is %d\n", DoALength );

    for( i=0; i<MAXNEWDIRNUM; i++ )
    {
        if(strlen(DirList[i]) == 0 )
           break; 
        if( FillDoA( DirList[i], _si, _DoA, &DoALength) != 0 ) //DoAlength will change so that it always points to the end of the _DoA array
        {
            printf( "FillDoA() error: i= %d, DoALength is %d, &DoALength is %d, *(&DoALength) is %d \n", i, DoALength, &DoALength, *(&DoALength));
            return -1;
        }
    }

    if ( PrintAllDoA(_DoA, "DoA.csv") != 0 ) //output the _DoA to a file 
    {
        printf( "PrintAllDoA() error!\n" );
        return -1;
    }

    return 0;
}

/************************************************************************************************************************************************* 
 * Function: check if the file _csv is what we need 
 * _csv: input parameter indicating a csv file name
 * _si: input parameter indicating the input/output information structure array
 * Return: 0: Yes, the file _csv is what we need
 *         -1: No, we don't need the file _csv
 *************************************************************************************************************************************************/
int CheckCSV(char *_csv, struct SMInfo *_si )
{
    char buff[MAXSTRINGSIZE];
    int i=0,j=0;
    FILE *fp = NULL; 

    //make sure the input parameters are not NULL otherwise we will be in big trouble
    if( _csv == NULL || _si == NULL )
    {
        printf( "CheckCSV() error: _csv=%s", _csv );
        return -1;
    }
    if( FileFilter(_csv) != 0 ) // we only need the *devc.csv or *evac.csv 
    {
        //printf( "FileFilter() error! _csv=%s\n", _csv );
        return -1;
    }

    fp=fopen(_csv,"r");
    if( fp == NULL )
    {
        printf( "fopen() error: _csv=%s", _csv );
        return -1;
    }

    fgets( buff, sizeof(buff),fp); //first line isn't needed 
    fgets( buff, sizeof(buff),fp); // we need the second line

    for( i=0; i<MAXLINENUM; i++ ) // search each line in the _si array
    {
       if( strlen(_si[i].VarType) == 0 ) // only non empty records in the _si array are considered 
           break;
       if( IsFileMatch(_csv, _si[i] ) != 0 )  // check if the filename includes the VarType and Alias provided in _si[i]
           continue;
       for( j=i;j<MAXLINENUM;j++) //output records are always behind the input records in SM_Info.txt
       {
           if( strlen(_si[j].VarType) == 0 ) // screen out the empty records
               break;
           if( _si[j].VarType[0] != 'O' ) // screen out the input records and only use the output records
               continue;
           if( strstr( buff, _si[j].FileVarName) != NULL ) // check if the head line of the csv file include the varname existing in _si[j]
           {
               fclose(fp);
               return 0;
           }
       }
    }

    fclose(fp);
    return -1;

}

/************************************************************************************************************************************************* 
 * Function: check if one _DoA record(_single_DoA)  has already been addressed in the sensitivity matrix record (_single_SMT)
 * _single_DoA: input parameter indicating one DoA record
 * _single_SMT: input parameter indicating one sensitivity matrix record
 * _rt: return value
 * Return: _rt=0: Yes, the _single_DoA has already been addressed in the _single_SMT
 *         -1: the left side value of the sensitivity matrix record is occupied
 *         1: the right side value of the sensitivity matrix record is occupied
 *************************************************************************************************************************************************/
void IsDoAInSMT( struct DoAlxInfo _single_DoA, struct SenMat _single_SMT, int *_rt )
{
    if( strcmp(trim(_single_DoA.InputVarType,NULL), trim(_single_SMT.InputVarType,NULL)) != 0||
        strcmp(trim(_single_DoA.InputAlias,NULL), trim(_single_SMT.InputAlias,NULL)) != 0||
        strcmp(trim(_single_DoA.InputFileVarName, NULL),trim(_single_SMT.InputFileVarName,NULL)) != 0||
        strcmp(trim(_single_DoA.InputBaseValue,NULL), trim(_single_SMT.InputBaseValue,NULL)) != 0|| 
        fabs(atof(_single_DoA.InputNewValue)-atof(_single_SMT.InputLeftValue)) < ZERO &&
        fabs(atof(_single_DoA.InputNewValue)-atof(_single_SMT.InputRightValue)) < ZERO  )
    {
        *_rt = 0;
        return;
    }
  
    if( fabs(atof(_single_SMT.InputLeftValue)) > ZERO )
        *_rt = -1;
    else 
        *_rt = 1;
    return;
}

/************************************************************************************************************************************************* 
 * Function: use the information from -single_doA to fill one side of the _single_SMT (for sensitivity analysis we need left side and right side)
 * _single_DoA: input parameter indicating a calculated analysis data record
 * _single_SMT: output parameter indicating a sensitivity matrix record
 * flag: 
 * Return:  void
 *************************************************************************************************************************************************/
void UpdateSMTOneSide( struct DoAlxInfo _single_DoA, struct SenMat *_single_SMT, int _flag )
{
    int k = 0;
    if( _flag == -1 ) //the left side has already been filled, then fill the right side
    {
        sprintf( _single_SMT->InputLeftValue, "%s", _single_DoA.InputNewValue );
        for(k=0;k<MAXOUTPUTSNUM;k++)
        {
           if( strlen(_single_DoA.OutputVarType[k] ) == 0 )
               break;
           _single_SMT->OutputLeftValue[k] = _single_DoA.OutputNewValue[k];
        }
    } else if (_flag == 1 ){ //the right side has already been filled, then fill the left side
        sprintf( _single_SMT->InputRightValue, "%s", _single_DoA.InputNewValue );
        for(k=0;k<MAXOUTPUTSNUM;k++)
        {
           if( strlen(_single_DoA.OutputVarType[k] ) == 0 )
               break;
           _single_SMT->OutputRightValue[k] = _single_DoA.OutputNewValue[k];
        }
    } else 
       printf( "wrong parameter: flag can only be -1 or 1, you pass flag=[%d]\n", _flag );

    return;
}

/************************************************************************************************************************************************* 
 * Function: insert a new record to the sensitivity matrix list
 * _single_DoA: input parameter indicating  one calculated analysis record
 * _SMT: output parameter indicating a sensitivity matrix list
 * _index: input parameter indicating the end position of the _SMT where a new record will be put in
 * Return: void
 *************************************************************************************************************************************************/
void InsertSMT( struct DoAlxInfo _single_DoA, struct SenMat *_SMT, int _index )
{
     int k=0, flag = 0;

     _SMT[_index].SenMatType = 0; //0 center, -1 backward, 1 forward 
     sprintf( _SMT[_index].InputVarType, "%s", _single_DoA.InputVarType );
     sprintf( _SMT[_index].InputAlias, "%s", _single_DoA.InputAlias);
     sprintf( _SMT[_index].InputFileVarName, "%s", _single_DoA.InputFileVarName);
     sprintf( _SMT[_index].InputBaseValue, "%s", _single_DoA.InputBaseValue);
     if( atof(_single_DoA.InputNewValue) - atof(_single_DoA.InputBaseValue)  > ZERO )
     { //if the input new value > the base value, then put the new value to the right side
         sprintf( _SMT[_index].InputRightValue, "%s", _single_DoA.InputNewValue);
         flag = 1;
     }
     else
     { //if the input new value < the base value, then put the new value to the left side
         sprintf( _SMT[_index].InputLeftValue, "%s", _single_DoA.InputNewValue);
         flag = -1;
     }
     
     for( k=0;k<MAXOUTPUTSNUM;k++ )
     { //deal with more than one output variables condition
         if( strlen(_single_DoA.OutputVarType[k]) == 0 )
             break;
         sprintf( _SMT[_index].OutputVarType[k], "%s", _single_DoA.OutputVarType[k]);
         sprintf( _SMT[_index].OutputAlias[k], "%s", _single_DoA.OutputAlias[k]);
         sprintf( _SMT[_index].TargetName[k], "%s", _single_DoA.TargetName[k]);
         sprintf( _SMT[_index].OutputFileVarName[k], "%s", _single_DoA.OutputFileVarName[k]);
         sprintf( _SMT[_index].comment[k], "%s", _single_DoA.comment[k]);
         _SMT[_index].OutputBaseValue[k]= _single_DoA.OutputBaseValue[k];
        
         if( flag == 1 ) // right side
             _SMT[_index].OutputRightValue[k]= _single_DoA.OutputNewValue[k];
         else  // left side
             _SMT[_index].OutputLeftValue[k]= _single_DoA.OutputNewValue[k];
     }

     return;
}

/************************************************************************************************************************************************* 
 * Function: Update an existing sensitivity matrix record by calling UpdateSMTOneSide()
 * _single_DoA: input parameter indicating one calculated analysis record
 * _SMT: output parameter indicating the sensitivity matrix list
 * Return: 0: success
 *         -1: failure due to the size limitation of sensitivity matrix  list
 *************************************************************************************************************************************************/
int UpdateSMT( struct DoAlxInfo _single_DoA, struct SenMat *_SMT )
{
    int i=0,rt=0;
    for( i=0;i<MAXINPUTSNUM; i++ )
    {
        if( strlen(_SMT[i].InputVarType) == 0 )
            break;
        IsDoAInSMT(_single_DoA,_SMT[i],&rt); 
        if( rt == -1 ) // the left side is already occupied, then update the right side
        {
           UpdateSMTOneSide(_single_DoA, &(_SMT[i]), 1);
           return 0;
        } else if ( rt == 1 )//the right side is already occupied, then update the left side
        {
           UpdateSMTOneSide(_single_DoA, &(_SMT[i]), -1);
           return 0;
        } 
    } 
    
    //at this time i should be at the end of the _SMT list
    if( i<MAXINPUTSNUM)
    {
        InsertSMT( _single_DoA, _SMT, i ); //insert one record to the _SMT list at the end of the _SMT list
        return 0;
    }else {
        printf( " MAXLINENUM is met: i= %d\n", i );
        return -1;
    }
}

/************************************************************************************************************************************************* 
 * Function: check if the two records are from the same FDS file which has been run more than one times to reduce the fluctuation of the FDS 
 *           simulation results
 *_single_i: input parameter indicating one DoA record
 *_single_j: input parameter indicating the other DoA record
 * Return: 0: Yes, the two records are from the same FDS file
 *         -1: No, they come from different FDS file
 *************************************************************************************************************************************************/
int IsRPDoA(struct DoAlxInfo _single_i, struct DoAlxInfo _single_j ) 
{
    if( strcmp(trim(_single_i.InputVarType,NULL),trim(_single_j.InputVarType,NULL)) != 0|| 
        strcmp(trim(_single_i.InputAlias,NULL),trim(_single_j.InputAlias,NULL)) != 0|| 
        strcmp(trim(_single_i.InputFileVarName,NULL),trim(_single_j.InputFileVarName,NULL)) != 0|| 
        strcmp(trim(_single_i.InputBaseValue,NULL), trim(_single_j.InputBaseValue,NULL) ) != 0|| 
        strcmp(trim(_single_i.InputNewValue,NULL),trim( _single_j.InputNewValue,NULL) )!=0 )
        return -1;

    return 0;
}

/************************************************************************************************************************************************* 
 * Function: calculate the sensitivity value and the change rate of each sensitivity record in _SMT
 * _SMT: input/output parameter indicating a snesitivity matrix list
 * Return:  void
 *************************************************************************************************************************************************/
void CalSMT( struct SenMat *_SMT )
{
    int i=0,k=0;
    for( i=0;i<MAXLINENUM; i++ )
    {
        double tmp_d=0.0;

        if( strlen(_SMT[i].InputVarType) == 0 )
            return;
        tmp_d = atof(_SMT[i].InputRightValue)-atof(_SMT[i].InputLeftValue);
        _SMT[i].ChangeRate = tmp_d/atof(_SMT[i].InputBaseValue);
        for( k=0;k<MAXOUTPUTSNUM; k++ )
            _SMT[i].Sensitivity[k] = (_SMT[i].OutputRightValue[k]-_SMT[i].OutputLeftValue[k])/tmp_d;
    }

    return;
}

/************************************************************************************************************************************************* 
 * Function: print the calculated sensitivity matrix  to a file (SMT.csv)
 * _SMT: input parameter indicating a sensitivity matrix list
 * _SMT_fn: input parameter indicating the name of a file to hold the _SMT list
 * Return: void
 *************************************************************************************************************************************************/
void PrintSMT( struct SenMat *_SMT , char * _SMT_fn )
{
    int i=0,k=0;
    char *str = "SenMatType, InputVarType, OutputVarType, InputAlias, OutputAlias, TargetName, InputFileVarName, OutputFileVarName, InputBaseValue, OutputBaseValue, InputLeftValue, InputRightValue, OutputLeftValue, OutputRightValue, Sensitivity, ChangeRate, comment\n";
    FILE *fp=fopen(_SMT_fn, "w+" );

    fputs( str, fp );

    for( i=0;i<MAXLINENUM; i++ )
    {
        char tmp_str[MAXSTRINGSIZE];
        if( strlen(_SMT[i].InputVarType) == 0 )
        {
            fclose(fp);
            return;
        }
        for( k=0; k<MAXOUTPUTSNUM; k++ )
        {
            if( strlen(_SMT[i].OutputVarType[k]) == 0 )
                break;

            memset(tmp_str, 0x0, sizeof(tmp_str));
            sprintf( tmp_str, "%d, %s, %s, %s, %s, %s, %s, %s, %s, %lf, %s, %s, %lf, %lf, %lf, %lf, %s\n",_SMT[i].SenMatType, _SMT[i].InputVarType, _SMT[i].OutputVarType[k], _SMT[i].InputAlias, _SMT[i].OutputAlias[k], _SMT[i].TargetName[k],_SMT[i].InputFileVarName, _SMT[i].OutputFileVarName[k], _SMT[i].InputBaseValue, _SMT[i].OutputBaseValue[k], _SMT[i].InputLeftValue, _SMT[i].InputRightValue, _SMT[i].OutputLeftValue[k], _SMT[i].OutputRightValue[k], _SMT[i].Sensitivity[k], _SMT[i].ChangeRate, _SMT[i].comment[k]); 
            fputs( tmp_str, fp );
            printf( tmp_str );
        }
    }
    fclose(fp);
    return;
}


/************************************************************************************************************************************************* 
 * Function: update a combined data analysis record from _single_DoA to the _CMB list. Note that _single_DoA and _CMB has the same structure and use  *            the same function
 * _single_DoA: input parameter indicating one single data analysis record
 * _CMB: output parameter indicating the combined fire scenario record list
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int UpdateCMB(struct DoAlxInfo _single_DoA, struct DoAlxInfo *_CMB)
{
    return UpdateRSM( _single_DoA, _CMB );
}

/************************************************************************************************************************************************* 
 * Function:  update a response surface method (RSM) data record from _single_DoA to the _RSM list. 
 * _single_DoA: input parameter indicating one single data analysis record
 * _RSM: output parameter indicating the record list used for response surface method 
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int UpdateRSM(struct DoAlxInfo _single_DoA, struct DoAlxInfo *_RSM)
{
    int i=0;

    for( i=0;i<MAXLINENUM; i++ )
    {
        if( strlen(_RSM[i].InputVarType) == 0 )
            break;
    }
    // at this time i should point to the end of the _RSM list
    if( i<MAXLINENUM )
    {
        int k=0, flag = 0, _index = i;

        sprintf( _RSM[_index].InputVarType, "%s", _single_DoA.InputVarType );
        sprintf( _RSM[_index].InputAlias, "%s", _single_DoA.InputAlias);
        sprintf( _RSM[_index].InputFileVarName, "%s", _single_DoA.InputFileVarName);
        sprintf( _RSM[_index].InputBaseValue, "%s", _single_DoA.InputBaseValue);
        sprintf( _RSM[_index].InputNewValue, "%s", _single_DoA.InputNewValue);

        for( k=0;k<MAXOUTPUTSNUM;k++ )
        {
            if( strlen(_single_DoA.OutputVarType[k]) == 0 )
                break;
            sprintf( _RSM[_index].OutputVarType[k], "%s", _single_DoA.OutputVarType[k]);
            sprintf( _RSM[_index].OutputAlias[k], "%s", _single_DoA.OutputAlias[k]);
            sprintf( _RSM[_index].TargetName[k], "%s", _single_DoA.TargetName[k]);
            sprintf( _RSM[_index].OutputFileVarName[k], "%s", _single_DoA.OutputFileVarName[k]);
            sprintf( _RSM[_index].comment[k], "%s", _single_DoA.comment[k]);
            _RSM[_index].OutputBaseValue[k]= _single_DoA.OutputBaseValue[k];
            _RSM[_index].OutputNewValue[k]= _single_DoA.OutputNewValue[k];
        }
        return 0;
    }else {
        printf( " MAXLINENUM is met: i= %d\n", i );
        return -1;
    }
    
    return 0;
}
/* *************************************************************************************************************************************************
 * Functions: find experimental data pairs from _RSM based on _IA and _OA for the use of power curve fitting
 * Input: 
 * _IA : InputAlias indicating the input variable, 
 * _OA: OutputAlias indicating the output variable, 
 * _RSM: includes the Response surfamce method needed information, 
 * _num: the array size of _x and _y
 * Output:
 * _x: the input value list;
 * _y: the output value list;
 * _num: the number of experimental data
 * return:
 * 0: success;
 * -1: failure ;
 **************************************************************************************************************************************************/ 
int GetXYFromRSM( char * _IA, char *_OA, struct DoAlxInfo *_RSM, double *_x, double *_y, int *_num)
{
    int i=0,j=0,k=0;

    if( _IA == NULL || _OA == NULL || _RSM == NULL || _x == NULL || _y == NULL || _num == NULL )
    {
        printf( "at least one of the parameters are NULL !\n");
        return -1;
    }

    for( i=0; i<MAXLINENUM; i++ )
    {
        if( strlen(trim(_RSM[i].InputVarType, NULL))==0)
            break;
        for( k=0; k<MAXOUTPUTSNUM; k++)
        {
            if( strlen(trim(_RSM[i].OutputVarType[k], NULL))==0)
                break;
            if( strcmp(_IA, _RSM[i].InputAlias)==0 && strcmp(_OA, _RSM[i].OutputAlias[k])==0 )
            { //for one input variable and one output variable, there maybe several records in RSM depending on the configuration file
                _x[j] = atof(_RSM[i].InputNewValue);
                _y[j] = _RSM[i].OutputNewValue[k];
                j++;
                if( j==*_num)
                {
                    printf( "too many experimental data: j=_num=%d, discard the left data set, i=%d,k=%d,inputVarType=%s,outputVarType=%f\n", j,i,k,_RSM[i].InputVarType, _RSM[i].OutputNewValue[k] );
                    return 0;
                }
            }
        }
    }
    *_num = j;
    return 0;
}

/************************************************************************************************************************************************* 
 * Function: this function is used to prepare a pair of input/output data for power curve fitting of one output to many inputs
 * _OA: input parameter indicating a series of output Alias seperated by "+"
 * _single_CMB: input parameter  holding the simulation data of combined fire scenarios 
 * _one_x: output parameter indicating product of input variables to their power: x[i]^b[i], initially *_one_x = 1.0
 * _one_y: output parameter indicating the simulation result of a combined fire scenario 
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int GetOneXY( char *_OA, struct DoAlxInfo _single_CMB, struct RSMResults *_RSMRlt, double *_one_x, double *_one_y )
{
    int i=0,j=0;
    
    for( i=0; i<MAXOUTPUTSNUM; i++ )
    {
        if( strcmp(_OA, _single_CMB.OutputAlias[i])== 0 )
        {
            struct VarInCol tmp_VIC[2];

            memset( tmp_VIC, 0x0, sizeof(tmp_VIC) );
           
            if( GetVICFromStrs(_single_CMB.InputAlias, _single_CMB.InputNewValue, tmp_VIC ) != 0 )
            {
                printf( "GetOneXY() error: GetViCFromStrs() error: InputAlias=%s, InputNewValue=%s \n", _single_CMB.InputAlias, _single_CMB.InputNewValue );
                return -1;
            }

            
            j=0;
            while( strlen(tmp_VIC[0].ColVal[j]) != 0 )
            {
                double tmp_a=0.0, tmp_b=0.0;
                double tmp_input_value = atof(tmp_VIC[1].ColVal[j]);

                if( GetParFromRSMRlt(tmp_VIC[0].ColVal[j], _OA, _RSMRlt, &tmp_a, &tmp_b ) == -1 )
                {
                    printf( "GetParFromRSMRlt()error! j=%d, _IA=%s, _OA=%s\n", j, tmp_VIC[0].ColVal[j], _OA );  
                    return -1;
                }
                *_one_x *=  pow(tmp_input_value,tmp_b);      
                j++;
            }
            *_one_y = _single_CMB.OutputNewValue[i];
        }
     }

     return 0;
}

/************************************************************************************************************************************************* 
 * Function: for one output variable/Alias (_OA), find a pair of input/output list of combined fire scenarios for power curve fitting
 * _csv: input parameter indicating a csv file name
 * _si: input parameter indicating the input/output information structure array
 * Return: 0: Yes, the file _csv is what we need
 *         -1: No, we don't need the file _csv
 *************************************************************************************************************************************************/
int GetXYFromCMBnRSMRlt(char *_OA, struct DoAlxInfo * _CMB, struct RSMResults *_RSMRlt, double *_x, double *_y, int *_num)
{
    int i=0,j=0;

    if( _OA == NULL || _CMB == NULL || _RSMRlt == NULL || _x == NULL || _y == NULL || _num == NULL )
    {
        printf( "GetXYFromCMBnRSMRlt() error!  one of the parameter pointer is NULL! _OA=%s, _num=%d\n", _OA, *_num );
        return -1;
    }
    for( i=0; i<*_num; i++ )
    {
        _x[i] = 1.00; //initialize the input list to 1.00
    }

    for( i=0; i<MAXLINENUM; i++)
    {
        double tmp_x=1.0, tmp_y=0.0;
        if( strlen(_CMB[i].InputVarType) == 0 )
           break;
        if( _CMB[i].flag != 2 )
           continue;
        if( GetOneXY(_OA, _CMB[i], _RSMRlt, &tmp_x, &tmp_y ) == -1 )
        {
            printf( "GetOneXY() error! _OA=%s, i=%d\n", _OA, i );
            return -1;
        }
        if( j < *_num )
        {
            _x[j] = tmp_x;
            _y[j] = tmp_y;
            j++;
        } else {
            printf( "GetXYFromCMBnRSMRlt() error! j=%d, _num=%d\n", j, *_num );
            return -1;
        }
    }

    *_num=j;
    return 0;
}

/************************************************************************************************************************************************* 
 * Functions: find the InputAlias matching OutputAlias. there maybe many records that matches _OA, but we only need one because all the InputAlias should be same given that the flag = 2
 * _OA: input parameter indicating output Alias
 * _CMB:input parameter indicating a calculated combined fire scenario record
 * _IA: output parameter indicating the input Alias
 * Return: 0: success
 *         -1: Failure
 *************************************************************************************************************************************************/
int GetInputAliasFromCMB(char *_OA, struct DoAlxInfo * _CMB, char *_IA )
{
    int i=0,k=0;
    for( i=0; i<MAXLINENUM; i++ )
    {
        if( strlen(_CMB[i].InputVarType) == 0 )
            break;
        if( _CMB[i].flag != 2 ) // only process the combined record where the inputalias include all the input variables seperated by "+"
            continue;

        for( k=0; k<MAXOUTPUTSNUM; k++ )
        {
           if( strcmp(_OA, _CMB[i].OutputAlias[k]) == 0 )
           {
               sprintf( _IA, "%s", _CMB[i].InputAlias );
               return 0;
           } 
        }
    }
    printf( "GetInputAlias() error! Cannot find _IA from _CMB! _OA = %s\n", _OA );
    return -1;
}

/************************************************************************************************************************************************* 
 * Function: fill _RSMRlt with the information from _RSM, _CMB, and _si.the main purpose is to calculate the parameters of power fitting curves between the inputAlias and OutputAlias. this function is the core of the RSM method which includes two levels of power curve fitting: 
 *        1. power curve fitting between one output and one input, namely how a single input could affect the result of one single output. the (x,y)pair list comes from _RSM  with information from _si being involved: y[j]=a[ij]*x[i]^b[ij], a[ij] and b[ij] will be calcualted for each input and output variable
 *        2. power curve fitting between one output and several/all input variables, namely how a combined fire scenairo with all input variables chaning at the same time could change the result of one output. This is done based on the first round curve fitting results, namely the b value of the regress equation: Y[j]=A[j]*X[j]^B[j] where X[j] *= (x[ij]^b[ij]), A[j] and B[j] will be calculated for each output variable.
 * _RSM: input parameter indicating the response surface method (RSM) record list
 * _CMB: input parameter indicating the combined fire scenarios record list
 * _si:  input parameter indicating the configuration file content
 * _RSMRlt: output parameter indicating the calcualted result of the RSM list
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int CalRSMRlt(struct DoAlxInfo *_RSM, struct DoAlxInfo *_CMB, struct SMInfo *_si, struct RSMResults *_RSMRlt)
{
    int i=0,j=0,k=0;
    struct VarInCol FDS_InputsVar[2];
    struct VarOutCol FDS_OutputsVar[2];

    memset( FDS_InputsVar, 0x0, sizeof(FDS_InputsVar));
    memset( FDS_OutputsVar, 0x0, sizeof(FDS_OutputsVar));
      
    if( GetVIC( _si, FDS_InputsVar ) != 0 ) // get the input variables and the input base values and save them into FDS_InputsVar
    {
        printf( "GetVIC() error !\n" );
        return -1;
    }
    if( GetVOC( _si, FDS_OutputsVar ) != 0 ) // get the output variables and the output base values and save them into FDS_OutputsVar
    {
        printf( "GetVOC() error !\n" );
        return -1;
    }
   
    //first round curve fitting:  this for sentence adds into _RSMRlt the records of power curving fitting parameters between one output and one single input
    for( i=0;i<MAXINPUTSNUM; i++ )
    {
        if( strlen(trim(FDS_InputsVar[0].ColVal[i],NULL))==0)
        {
            break;
        }
        for( j=0; j<MAXOUTPUTSNUM; j++)
        {
            int num=128;
            double tmp_x[128], tmp_y[128];
            double tmp_A=0.0, tmp_B=0.0, tmp_R_sqr=0.0, tmp_S_sqr=0.0;

            memset( tmp_x, 0x0, sizeof(tmp_x));
            memset( tmp_y, 0x0, sizeof(tmp_y));

            
            if( strlen(FDS_OutputsVar[0].ColVal[j]) == 0)
                break;
            if( GetXYFromRSM(FDS_InputsVar[0].ColVal[i], FDS_OutputsVar[0].ColVal[j], _RSM, tmp_x, tmp_y, &num ) != 0 )
            {
                printf( "GetXYFromRSM() error!\n, i=%d, j=%d, inputAlias=%s, Output Alias=%s\n",i, j, FDS_InputsVar[0].ColVal[i], FDS_OutputsVar[0].ColVal[j]);
                return -1;
            }

            PowerFit( tmp_x, tmp_y, num, &tmp_A, &tmp_B, &tmp_R_sqr, &tmp_S_sqr);

            sprintf( FDS_RSMResults[k].OutputAlias,"%s", FDS_OutputsVar[0].ColVal[j] );
            sprintf( FDS_RSMResults[k].InputAlias,"%s", FDS_InputsVar[0].ColVal[i] );
            FDS_RSMResults[k].OutputBaseValue = atof(FDS_OutputsVar[1].ColVal[j]);
            FDS_RSMResults[k].a = tmp_A;
            FDS_RSMResults[k].b = tmp_B;
            FDS_RSMResults[k].r_sqr = tmp_R_sqr;
            FDS_RSMResults[k].s_sqr = tmp_S_sqr;
            k++;
        }
    }


    // Second round curve fitting: this for sentence adds records of power curve fitting parameter between one output and many inputs
    for( j=0; j<MAXOUTPUTSNUM; j++)
    {
         int num=128;
         double tmp_x[128], tmp_y[128];
         double tmp_A=0.0, tmp_B=0.0, tmp_R_sqr=0.0, tmp_S_sqr=0.0;

         memset( tmp_x, 0x0, sizeof(tmp_x));
         memset( tmp_y, 0x0, sizeof(tmp_y));

         if( strlen(trim(FDS_OutputsVar[0].ColVal[j],NULL))==0)
             break;

         if( GetXYFromCMBnRSMRlt(FDS_OutputsVar[0].ColVal[j], _CMB, FDS_RSMResults, tmp_x, tmp_y, &num ) != 0 )
         {
             printf( "GetXYFromCMBnRSMRlt() error!, j=%d, Output Alias=%s\n", j, FDS_OutputsVar[0].ColVal[j] );
             return -1;
         }

         PowerFit( tmp_x, tmp_y, num, &tmp_A, &tmp_B, &tmp_R_sqr, &tmp_S_sqr);

         sprintf( FDS_RSMResults[k].OutputAlias,"%s", FDS_OutputsVar[0].ColVal[j] );
         GetInputAliasFromCMB(FDS_OutputsVar[0].ColVal[j], _CMB, FDS_RSMResults[k].InputAlias );

         FDS_RSMResults[k].OutputBaseValue = atof(FDS_OutputsVar[1].ColVal[j]);
         FDS_RSMResults[k].a = tmp_A;
         FDS_RSMResults[k].b = tmp_B;
         FDS_RSMResults[k].r_sqr = tmp_R_sqr;
         FDS_RSMResults[k].s_sqr = tmp_S_sqr;
         k++;
    }

    return 0;
}

/************************************************************************************************************************************************* 
 * Function: extract the sensitivity matrix information from _SMT which is a detailed sensitivity data source
 * _SMT: input parameter indicating detailed sensitivity data source
 * _sen_matx: output parameter indicating the sensitivity matrix
 * _SMT_fn: input parameter indicating a file holding the sensitivity matrix data
 * Return: void
 *         
 *************************************************************************************************************************************************/
void GetSenMatx(struct SenMat *_SMT, char _sen_matx[MAXINPUTSNUM+1][MAXOUTPUTSNUM+1][128], char *_SMT_fn )
{
    int i=0;
    FILE *fp=fopen(_SMT_fn, "w+" );

    sprintf(_sen_matx[0][0], "%s", "dy/dx" );

    //search sensitivity values from _SMT  and put them into the SenMatx  list 
    for( i=0; i<= MAXINPUTSNUM; i++ )
    {
        int j=0;

        if( i>0 )
        {
            if( strlen(_SMT[i-1].InputVarType) == 0 )
                break;
        } else {
            if( strlen(_SMT[i].InputVarType) == 0 )
                break;
        }

        for( j=0; j<= MAXOUTPUTSNUM; j++ )
        {
           if( i==0 && j==0 )
           {
               sprintf(_sen_matx[i][j], "%s", "dy/dx" );
               printf( "_sen_matx[%d][%d]=[%s]\n", i,j,_sen_matx[i][j] );
               continue;
           }
           if( i==0 && j>0 )
           {
               if( strlen(_SMT[i].OutputAlias[j-1]) == 0 )
                   break;
               sprintf(_sen_matx[i][j], "%s", _SMT[i].OutputAlias[j-1] );
               printf( "_sen_matx[%d][%d]=[%s]\n", i,j,_sen_matx[i][j] );
               continue;
           }
           if( i>0 && j==0 )
           {
               if( strlen(_SMT[i-1].InputAlias) == 0 )
                   break;
               sprintf(_sen_matx[i][j], "%s", _SMT[i-1].InputAlias );
               printf( "_sen_matx[%d][%d]=[%s]\n", i,j,_sen_matx[i][j] );
               continue;
           }
           if( i>0 && j>0 )
           {
               if( fabs(_SMT[i-1].Sensitivity[j-1] ) < ZERO )
                   break;
               sprintf(_sen_matx[i][j], "%lf", _SMT[i-1].Sensitivity[j-1]);
               printf( "_sen_matx[%d][%d]=[%s]\n", i,j, _sen_matx[i][j] );
               continue;
           }
        }
        
    }

    printf( "Senmatx is below:\n" );
    // print the sensitivity matrix to SMT.csv
    for( i=0; i<=MAXINPUTSNUM; i++ )
    {
        int j=0;

        if( i> 0 )
        {
            if( strlen(_SMT[i-1].InputVarType) == 0 )
                break;
        }else {
            if( strlen(_SMT[i].InputVarType) == 0 )
                break;
        }

        for( j=0; j<=MAXOUTPUTSNUM; j++ )
        {
            if( i>0 && j> 0 )
            {
                if( fabs(_SMT[i-1].Sensitivity[j-1] ) < ZERO )
                    break;
            }
            if( strlen(_sen_matx[i][j]) > 0 )
            {
                printf( "%s\t", _sen_matx[i][j] );
                if( j == 0 )
                    fprintf(fp, "%s", _sen_matx[i][j] );
                else 
                    fprintf(fp, ",%s", _sen_matx[i][j] );
            }
        }
        printf( "\n");
        fprintf(fp,"\n");
    }
    fclose(fp);
    return;
}

/************************************************************************************************************************************************* 
 * Function: find the sensitivity of _OutputAlias to _InputAlias from SenMatx 
 * _InputAlias: input parameter indicating an input variable name (Alias)
 * _OutputAlias: input parameter indicating an output variable name (Alias)
 * _sen_matx: input parameter indicating a sensitivity matrix
 * _OneSen: output parameter indicating  one sensitivity 
 * Return: 0: success
 *         -1: Failure
 *************************************************************************************************************************************************/
int RtOneSen(char *_InputAlias, char *_OutputAlias, char _sen_matx[MAXINPUTSNUM+1][MAXOUTPUTSNUM+1][128], double *_OneSen )
{
    int i=0,k=0;
    int flag = 0;

    // find the location of the input variable matching _InputAlias
    for( i=0; i<=MAXINPUTSNUM;i++ )
    {
         if( strcmp( _sen_matx[i][0], _InputAlias ) == 0 )
         {
             flag++ ;
             break;
         }
    }

    // find the location of the output variable matching _OutputAlias
    for( k=0; k<=MAXOUTPUTSNUM; k++ )
    {
         if( strcmp( _sen_matx[0][k], _OutputAlias ) == 0 )
         {
             flag++ ;
             break;
         }
    }

    if( flag == 2 ) //both input and output are found 
    {
        *_OneSen = atof(_sen_matx[i][k]);
        return 0;
    } else {
        printf( "RtOneSen() error: cannot find the sensitivity: flag=[%d], _InputAlias=[%s], _OutputAlias[%s]\n", flag, _InputAlias, _OutputAlias );
        return -1;    
    }
}

/************************************************************************************************************************************************* 
 * Function: this function calculates the value of outputalias predicted by the sensitivity matrix method and put them in the PreValueSMT filed to compare with the output results from FDS simulation
 * _CMB: input/output parameter indicating analysis data of combined fire scenairo
 * _sen_matx: input parameter indicating the sensitivity matrx
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int CalCMB(struct DoAlxInfo *_CMB, char _sen_matx[MAXINPUTSNUM+1][MAXOUTPUTSNUM+1][128] )
{
    int i=0,j=0;
    struct DoAlxInfo *tmp_smt = FDS_SenMat;
    
    // set flag back to 0 for next step usage
    for( i=0; i<MAXLINENUM; i++ )
    {
       if( strlen(_CMB[i].InputVarType) == 0 )
           continue;
       else 
           _CMB[i].flag = 0; //unprocessed 
    }
    for( i=0; i<MAXLINENUM; i++ ) //for every record in _CMB
    {
        int tmp_i=0, tmp_k=0;
        char tmp_VarType[4];
        double tmp_gap[MAXOUTPUTSNUM];

        printf( "in CalCMB before for: _CMB[%d].flag = [%d]\n", i, _CMB[i].flag );

        if( _CMB[i].flag > 0 )
            continue;

        if( strlen(_CMB[i].InputVarType) == 0 )
            break;

        memset( tmp_gap, 0x0, sizeof(tmp_gap) );

        // add the first addition from the first variable to the gap
        for( tmp_k=0; tmp_k<MAXOUTPUTSNUM; tmp_k++ )
        {
             double tmp_rt=0.0;
             if( RtOneSen(_CMB[i].InputAlias, _CMB[i].OutputAlias[tmp_k],_sen_matx, &tmp_rt) != 0 )
             {
                 printf( "CalCMB() error!\n" );
                 return -1;
             } else {
                 tmp_gap[tmp_k] = tmp_rt*(atof(_CMB[i].InputNewValue)-atof(_CMB[i].InputBaseValue));
             }
        }

        for( j=i+1; j<MAXLINENUM; j++ )
        {
            // for combined records, the sentence make sure that the first record of a group with the same InputVarType include all the input variables  and inut values and put the prediction results by sensitivity matrix into comment
            if( strcmp(_CMB[i].InputVarType, _CMB[j].InputVarType) == 0 )
            {
                double tmp_input_gap= atof(_CMB[j].InputNewValue)-atof(_CMB[j].InputBaseValue);
                for( tmp_k=0; tmp_k<MAXOUTPUTSNUM; tmp_k++ )
                {
                    double tmp_rt=0.0;
                    if( RtOneSen(_CMB[j].InputAlias, _CMB[j].OutputAlias[tmp_k],_sen_matx, &tmp_rt) != 0 )
                    {
                        printf( "CalCMB() error!\n" );
                        return -1;
                    } else {
                       tmp_gap[tmp_k] += tmp_input_gap*tmp_rt;
                    }
                }
             
                strcat(_CMB[i].InputAlias, "+" );
                strcat(_CMB[i].InputAlias, _CMB[j].InputAlias);
                strcat(_CMB[i].InputFileVarName, "+" );
                strcat(_CMB[i].InputFileVarName, _CMB[j].InputFileVarName );
                strcat(_CMB[i].InputNewValue, "+" );
                strcat(_CMB[i].InputNewValue, _CMB[j].InputNewValue);
                _CMB[j].flag = 1;
            }
        }

        //put the predicted output results into  PreValueSMT
        for( tmp_k=0; tmp_k<MAXOUTPUTSNUM; tmp_k++ )
        {
             //sprintf( _CMB[i].comment[tmp_k], "%lf", _CMB[i].OutputBaseValue[tmp_k]+tmp_gap[tmp_k] );
             _CMB[i].PreValueSMT[tmp_k] = _CMB[i].OutputBaseValue[tmp_k]+tmp_gap[tmp_k];
             printf( "_CMB[%d].OutputNewValue[%d]=[%lf], _CMB[%d].PrevalueSMT[%d]=[%lf]\n", i, tmp_k, _CMB[i].OutputNewValue[tmp_k], i, tmp_k, _CMB[i].PreValueSMT[tmp_k] );
        }

        _CMB[i].flag = 2;  //for InputVarType[2]='C', namely combined records, only records with flag=2 are output to CMB.csv
    }

    return 0;
}

//print the record array of RSMResults  to file _RSMRlt_fn.
int PrintRSMRlt( struct RSMResults *_RSMRlt, char * _RSMRlt_fn)
{
    int i=0;
    FILE *fp=fopen(_RSMRlt_fn, "w+");
    if( fp==NULL )
    {
        printf( "fopen() error! _RSMRlt_fn=[%s]\n", _RSMRlt_fn );
        return -1;
    }

    fprintf( fp, "OutputAlias, InputAlias, a, b, r_sqr, s_sqr, OutputBaseValue\n");
    do{
        char tmp_str[MAXSTRINGSIZE];
        memset(tmp_str, 0x0, sizeof(tmp_str));
        sprintf( tmp_str, "%s,%s,%.2f,%.2f,%.6f,%.6f,%.2f\n", _RSMRlt[i].OutputAlias, _RSMRlt[i].InputAlias, _RSMRlt[i].a, _RSMRlt[i].b, _RSMRlt[i].r_sqr, _RSMRlt[i].s_sqr, _RSMRlt[i].OutputBaseValue );
        fprintf( fp, tmp_str );
        //printf( tmp_str );
        i++;
    }while (strlen(trim(_RSMRlt[i].OutputAlias,NULL)) !=0 );

    fclose(fp);
    return 0;
}

//print the record array of RSM(response surface method)  to file _RSM_fn. Note that _RSM has the same struct type as CMB and DoA
int PrintRSM( struct DoAlxInfo *_RSM, char * _RSM_fn )
{
    return PrintAllDoA( _RSM, _RSM_fn);
}

//print one record combined fire scenario record _C to _fp.
void PrintOneCMB(struct DoAlxInfo _C, FILE * _fp ) 
{
     int i=0;

     for( i=0; i<MAXOUTPUTSNUM; i++ )
     {
         char tmp_str[MAXSTRINGSIZE];

         if( strlen(_C.OutputVarType[i]) == 0 )
             return;

         memset(tmp_str, 0x0, sizeof(tmp_str));
         sprintf( tmp_str, "%s, %s, %s, %s, %s, %s, %s, %s, %.2f, %s, %.2f, %.2f,%.2f\n", _C.InputVarType, _C.OutputVarType[i], _C.InputAlias, _C.OutputAlias[i],_C.TargetName[i], _C.InputFileVarName, _C.OutputFileVarName[i], _C.InputBaseValue, _C.OutputBaseValue[i], _C.InputNewValue, _C.OutputNewValue[i], _C.PreValueRSM[i], _C.PreValueSMT[i]); 
         fputs( tmp_str, _fp );
         printf( "_C.flag=%d,_C.OutputVarType[%d]=%s, _C.OutputAlias[%d]=%s,_C.OutputFileVarName[%d]=%s,_C.OutputBaseValue[%d]=%lf,_C.OutputNewValue[%d]=%lf, _C.PreValueRSM[%d]=[%lf],_C.PreValueSMT[%d]=[%lf]\n",_C.flag, i, _C.OutputVarType[i], i,_C.OutputAlias[i], i,_C.OutputFileVarName[i], i,_C.OutputBaseValue[i], i, _C.OutputNewValue[i], i, _C.PreValueRSM[i], i,_C.PreValueSMT[i]);
     }
     return;
}

//print the combined fire scenario record array _CMB to _CMB_fn. note that _CMB has same struct type as DoA
int PrintCMB( struct DoAlxInfo *_CMB, char * _CMB_fn )
{
     int i = 0;
     char *tmp_str = "InputVarType, OutputVarType, InputAlias, OutputAlias,TargetName, InputFileVarName, OutputFileVarName, InputBaseValue, OutputBaseValue, InputNewValue, OutputNewValue, PreValueRSM, PreValueSMT\n";
     FILE *fp=fopen(_CMB_fn, "w+" );

     if( fp == NULL )
     {
         printf( "PrintCMB() error: cannnot open [%s]\n", _CMB_fn );
         return -1;
     }
     fputs( tmp_str, fp );
     for( i=0;i<MAXLINENUM;i++)
     {
         printf( "_CMB[%d].InputVarType=[%s],_cmb[%d].flag=[%d]\n", i,_CMB[i].InputVarType, i,_CMB[i].flag );

         if( strlen(_CMB[i].InputVarType) == 0 )
             break;
         if( _CMB[i].flag != 2 )
             continue;
         PrintOneCMB(_CMB[i], fp);
     }
     fclose( fp );
     return;
}

/************************************************************************************************************************************************* 
 * Function: for each combined fire scenario record, calculate the prediction of the RSM and put it to _CMB, then we can compare the FDS simulation results with the predictions of RSM and SMT.
 * _CMB: input/output parameter indicating a combined fire scenairo record array
 * _RSMRlt: input parameter indicating the parameters of powrer curve fitting (RSM)
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int CmpRSMnSMT( struct DoAlxInfo *_CMB, struct RSMResults *_RSMRlt )
{
    int i=0,j=0,k=0;
    for( i=0; i<MAXLINENUM; i++ )
    {
        if( strlen(_CMB[i].InputVarType) == 0 )
            break;
        if(_CMB[i].flag != 2 )
           continue;
        for( j=0; j<MAXLINENUM; j++ )
        {
            if( strlen(_RSMRlt[j].OutputAlias) == 0 )
                break;
            for( k=0; k<MAXOUTPUTSNUM; k++ )
            {
               if( strcmp(_CMB[i].InputAlias, _RSMRlt[j].InputAlias) == 0 && strcmp(_CMB[i].OutputAlias[k], _RSMRlt[j].OutputAlias) == 0 )
               {
                   double tmp_one_pv = 0.0;
                   if( GetOnePvFromRSMRlt(_CMB[i].InputAlias, _CMB[i].InputNewValue, _CMB[i].OutputAlias[k], _RSMRlt, &tmp_one_pv) != 0 )
                   {
                       printf( "GetOnePvFromRSMRlt() error! InputAlias=%s, InputNewValue=%s, OutputAlias=%s\n", _CMB[i].InputAlias, _CMB[i].InputNewValue, _CMB[i].OutputAlias[k] );
                       return -1;
                   }   
                   _CMB[i].PreValueRSM[k] = tmp_one_pv;
               }
            }
        }
    }
    return 0;
}

/************************************************************************************************************************************************* 
 * Function: generate records for Response surface method (RSM,VarType="IR*" ) and combined fire scenairos (VarType = "**C" ).
 *     Flow chat: 
 *     1. reset the flag to zero for the following uses in this function
 *     2. for each  line in _DoA, check if its VarType to make sure we will process it.
 *     3. check if its flag == 1, Yes means this record has already been averaged by another record with the same input information, otherwise set it to 1
 *     4. combine and average the records with same input information
 *     5. update the calculated tmp-DoA to _RSM or _CMB
 *     6. calculate  the output values predicted  by SMM (CalCMB()) and put it into CMB to compare with the FDS simulation results
 *     7. calculate  the parameters of power fitting functionsa (CalRSMRlt())
 *     8. calculate  the output values predicted by RSM and put it into CMB to compare with the FDS simulation results and the SMM predicted values
 *     9. print RSM, RSMRlt, and CMB
 * _DoA: input parameter indicating a calculated output array based on input informaiton
 * _si: input parameter indicating the input information structure array
 * _RSM: output parameter indicating the input information structure array
 * _RSMRlt: output parameter holding the parameters of power fitting functions
 * _CMB: output parameter holding the compared results of FDS simulation, SMM and RSM
 * Return: 0: success
 *         -1: Failure
 *************************************************************************************************************************************************/
int GenRSMnCMB( struct DoAlxInfo *_DoA, struct SMInfo *_si, struct DoAlxInfo *_RSM, struct RSMResults *_RSMRlt, struct DoAlxInfo *_CMB )
{
    
    int i=0,j=0; 
    
    for( i=0; i<MAXLINENUM; i++ )
    {
        //reset the flag. the users should check the DoA.csv if there are some records with flag = 1 which means the FDs simulation time maybe too short for the critical value to be met
        _DoA[i].flag = 0 ; 
    }

    for( i=0; i<MAXLINENUM; i++ )
    {
        double tmp_final_d[MAXOUTPUTSNUM];
        struct DoAlxInfo tmp_DoA;
        int tmp_k=0;

        memset( tmp_final_d, 0x0, sizeof(tmp_final_d) );
        memset( &tmp_DoA, 0x0, sizeof(tmp_DoA) );

        memcpy( &tmp_DoA, &(_DoA[i]), sizeof(tmp_DoA));

        if( strstr(_DoA[i].InputVarType, "IR" ) == NULL && _DoA[i].InputVarType[2] != 'C' )
            continue;

        if( strstr(_DoA[i].InputVarType, "IR" ) != NULL )
            printf( "begin UpdateRSM(), i=%d, tmp_DoA.flag=%d, tmp_DoA.InputVarType=%s,tmp_DoA.InputAlias=%s, tmp_DoA.InputNewValue=%s, tmp_DoA.OutputNewValue=%lf\n", i, tmp_DoA.flag,tmp_DoA.InputVarType, tmp_DoA.InputAlias, tmp_DoA.InputNewValue, tmp_DoA.OutputNewValue[0] );

        // flag = 1 : the record has been processed; flag = 0: not yet
        if( _DoA[i].flag == 1 ) 
            continue;
        else 
            _DoA[i].flag = 1 ;

        for(tmp_k=0;tmp_k<MAXOUTPUTSNUM; tmp_k++ )
            tmp_DoA.OutputNewValue[tmp_k] = _DoA[i].OutputNewValue[tmp_k]; 

        // there are repeated FDS simulations, namely _DoA include repeated records, we nned to average these repeated records
        if( RPNUMSNR > 0 || RPNUMCMB > 0 )
        {
            int count=0;
            for( j=i+1;j<MAXLINENUM;j++)
            {
               if( _DoA[j].flag == 1 || strstr(_DoA[j].InputVarType, "IR") == NULL && _DoA[j].InputVarType[2] != 'C' )
                   continue;
               if( IsRPDoA(_DoA[i], _DoA[j] ) == 0 )
               {
                   for(tmp_k=0;tmp_k<MAXOUTPUTSNUM; tmp_k++ )
                       tmp_DoA.OutputNewValue[tmp_k] += _DoA[j].OutputNewValue[tmp_k]; 
                   _DoA[j].flag = 1;
                   count++;
                   printf( "count=[%d],i=[%d],j=[%d],tmp_DoA.OutputNewValue[0]=[%lf]\n", count, i,j, tmp_DoA.OutputNewValue[0] );
               }
            }
            for(tmp_k=0;tmp_k<MAXOUTPUTSNUM; tmp_k++ )
                tmp_DoA.OutputNewValue[tmp_k] /= count+1;
            printf( "count=[%d],tmp_DoA.OutputNewValue[0]=[%lf]\n", count, tmp_DoA.OutputNewValue[0] );
        }

        if( strstr(tmp_DoA.InputVarType, "IR") != NULL )
        {
           
            printf( "before UpdateRSM(), tmp_DoA.InputVarType=%s,tmp_DoA.InputAlias=%s, tmp_DoA.InputNewValue=%s, tmp_DoA.OutputNewValue=%lf\n", tmp_DoA.InputVarType, tmp_DoA.InputAlias, tmp_DoA.InputNewValue, tmp_DoA.OutputNewValue[0] );
            if ( UpdateRSM(tmp_DoA, _RSM) != 0 )
            {
                printf( "UpdateRSM() error !\n" );
                return -1;
            }
        }
        else 
        {
            if( UpdateCMB(tmp_DoA, _CMB) != 0 )
            {
                printf( "UpdateCMB() error !\n" );
                return -1;
            }
        }

    }

    if( CalCMB(_CMB, SenMatx ) != 0 ) 
    {
        printf( "CalCMB() error!\n" );
        return -1;
    }

    if( CalRSMRlt(_RSM, _CMB, _si, _RSMRlt) != 0 )
    {
        printf( "CalRSMRlt() error!\n");
        return -1;
    }

    if( CmpRSMnSMT(_CMB, _RSMRlt ) != 0 )
    {
        printf( "CmpRSMnSMT() error!\n" );
        return -1;
    }

    if( PrintRSMRlt( _RSMRlt, "RSMRlt.csv" )!= 0)
    {
        printf( "PrintRSMRlt() error!\n");
        return -1;
    }

    if( PrintRSM( _RSM, "RSM.csv") != 0 )
    {
        printf( "PrintRSM() error !\n" );
        return -1;
    }

    if( PrintCMB( _CMB, "CMB.csv") )
    {
        printf( "PrintCMB() error !\n" );
        return -1;
    }

    return 0;
    
}

/************************************************************************************************************************************************* 
 * Function: generate sensitivity matix based on the simulation result array _DoA
 * _DoA: input parameter indicating a calculated output array based on input informaiton
 * _SMT: input parameter holding the detail information about the sensitivity matrix
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int GenSMT( struct DoAlxInfo *_DoA, struct SenMat *_SMT )
{
    int i=0,j=0; 
    
    for( i=0; i<MAXLINENUM; i++ )
    {
        double tmp_final_d[MAXOUTPUTSNUM];
        struct DoAlxInfo tmp_DoA;
        int tmp_k=0;

        memset( tmp_final_d, 0x0, sizeof(tmp_final_d) );
        memset( &tmp_DoA, 0x0, sizeof(tmp_DoA) );

        memcpy( &tmp_DoA, &(_DoA[i]), sizeof(tmp_DoA));

        if( strstr(_DoA[i].InputVarType, "IS" ) == NULL )
            continue;
        // flag = 1 : either the FDS simulation is too short for the critical value to be met or the record has been transfered into sensitivity matrix; flag = 0: not yet
        if( _DoA[i].flag == 1 ) 
            continue;
        else 
        {
            printf( "RPNUMSNR: _DoA[%d].InputVarType=%s,_DoA[%d].flag=%d\n", j,_DoA[j].InputVarType,j, _DoA[j].flag );
            _DoA[i].flag = 1 ;
        }

        for(tmp_k=0;tmp_k<MAXOUTPUTSNUM; tmp_k++ )
            tmp_DoA.OutputNewValue[tmp_k] = _DoA[i].OutputNewValue[tmp_k]; 

        if( RPNUMSNR > 0 ) // there are repeated FDS simulations, we need to average the output values because the output maybe different when an identical FDS file is run two times due to the random properties of FDS simulation
        {
            int count=0;
            for( j=i+1;j<MAXLINENUM;j++)
            {
               if( _DoA[j].flag == 1 || strstr(_DoA[j].InputVarType, "IS") == NULL )
                   continue;
               if( IsRPDoA(_DoA[i], _DoA[j] ) == 0 )
               {
                   for(tmp_k=0;tmp_k<MAXOUTPUTSNUM; tmp_k++ )
                       tmp_DoA.OutputNewValue[tmp_k] += _DoA[j].OutputNewValue[tmp_k]; 
                   _DoA[j].flag = 1;
                   printf( "RPNUMSNR: _DoA[%d].InputVarType=%s,_DoA[%d].flag=%d\n", j,_DoA[j].InputVarType,j, _DoA[j].flag );
                   count++;
               }
            }
            for(tmp_k=0;tmp_k<MAXOUTPUTSNUM; tmp_k++ )
                tmp_DoA.OutputNewValue[tmp_k] /= count+1;
        }

        if ( UpdateSMT(tmp_DoA, _SMT) != 0 )
        {
           printf( "updateSMT() error!\n" );
           return -1;
        }
    }

    CalSMT(_SMT); // calculate the sensitivity matix details
    GetSenMatx(_SMT, SenMatx, "SMT.csv"); // save the sensitivity matrix to SenMatx and the file of "SMT.csv"
    PrintSMT( _SMT, "SMT_detail.csv"); // save the sensitivity matrix details to  the file of "SMT_detail.csv"
    return 0;
}

int main( int argc, char ** argv )
{
    char *tmp_ret=NULL;
    
    if ( argc != 2 )
    {
        int i=0;
        for ( i=0; i<argc; i++ )
           printf( "%s\n", argv[i] );
        printf( "only one argument is needed, you have [%d] arguments\n" , argc);
        return -1;
    }
    memset( FDS_SmInfo, '\0', sizeof(FDS_SmInfo));
    memset( FDS_DoA, '\0', sizeof(FDS_DoA));
    memset( FDS_SenMat, '\0', sizeof(FDS_SenMat));
    memset( FDS_RSM, '\0', sizeof(FDS_RSM));
    memset( FDS_CMB, '\0', sizeof(FDS_CMB));
    memset( SenMatx, '\0', sizeof(SenMatx));

    if ( readin(argv[1], FDS_SmInfo) != 0 ){ 
        printf( "read SMInfo to structure error!\n" );
        return -1;
    }
    if( GenDoA( FDS_SmInfo, FDS_DoA ) != 0 )
    {
        printf( "GenDoA() error!\n" );
        return -1;
    }
    if( GenSMT( FDS_DoA, FDS_SenMat ) != 0)
    {
        printf( "GenSMT() error!\n" );
        return -1;
    }
    if( GenRSMnCMB( FDS_DoA, FDS_SmInfo, FDS_RSM, FDS_RSMResults, FDS_CMB )!= 0 )
    {
        printf( "GenRSMnCMB() error !\n" );
        return -1;
    }

    return 0;
}
