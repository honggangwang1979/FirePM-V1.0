
/****************************************************************************************************************************************************
 *  Author: Honggang Wang and Professor Dembsey, the Department of Fire Protection Engineering in WPI
 *   * Copyright: This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation with the author information being cited
 *
 * Discription: this file includes the source code of the tool, FirepM, which can be used to dynamically monitor the change of building fire performance defined by the user (ASET,RSET, etc) based on the change of input data (Dyn.txt)
 *
 * How to Run this tool: ./FirePM SM_Info.txt.  this tool can be assisted by another tool, ./GSD, which can generate simulation data and save the data to input data file (Dyn.txt), and then FirePM will check the change of the Dyn.txt and output the change of building fire performance 
 *
 * Flowchat: 
 *     step 1 -> read information from user's input file (SM_Info.txt) into  a SMInfo struct array (FDS_SmInfo)
 *     step 2 -> read in the input base data and the output base data
 *     step 3 -> in a while, repeatedly check if the sensitivity file (SMT.txt) and the input data file (Dyn.txt)  are updated, if Yes then recalculate the change of the building fire performance
 ***************************************************************************************************************************************************/

#include "FirePM.h"

struct ThreeDCoordinate tmp_3DC[3];
struct GenInfo FDS_GenInfo[MAXFILENUM];
struct SMInfo FDS_SmInfo[MAXLINENUM];
struct VarInCol FDS_InputsVar[2];
struct VarOutCol FDS_OutputsVar[2];
struct VarInCol FDS_DynIn[MAXLINENUM];
struct VarOutCol FDS_OutputsRltSMT[MAXLINENUM];
struct VarOutCol FDS_OutputsRltRSM[MAXLINENUM];
struct RSMResults FDS_RSMResults[(MAXINPUTSNUM+1)*(MAXOUTPUTSNUM+1)]; //to store the results of Response Surface Method


/************************************************************************************************************************************************* 
 * Function: extract one input new value matching _IA and IBV from the dynamic input list (_DynIn)
 * _IA: input parameter indicating the inputAlias  
 * _IBV : input parameter indicating the input base value
 * _DynIn: dynamically updated input data, 
 * _k: input parameter indicating the line number of dynamically updated input data (Dyn.txt), namely _DynIn; 
 * _Inv: output parameter holding the found input value
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int FindOneINV(char *_IA, char * _IBV, struct VarInCol *_DynIn, int _k, double *_Inv )
{
    int i=0;

    if( _IA == NULL || _IBV == NULL || _DynIn == NULL || _Inv == NULL )
    {
        printf( "at least one of the input pointers are NULL!please check them\n"  );
        return -1;
    }

    for( i=0;i<MAXINPUTSNUM; i++ )
    {
        //if( strstr(_IA, _DynIn[0].ColVal[i]) != NULL )
        if( strcmp(_IA, _DynIn[0].ColVal[i]) == 0)
        {
            if( strstr(_DynIn[_k].ColVal[i], "|" ) == NULL ) // non-geographic input variables 
                *_Inv = atof(_DynIn[_k].ColVal[i]);
            else {// geographic input variables 
                struct ThreeDCoordinate tmp_3d_base, tmp_3d_new;
                memset( &tmp_3d_base, 0x0, sizeof(tmp_3d_base));
                memset( &tmp_3d_new, 0x0, sizeof(tmp_3d_new));
                sscanf(_IBV,"%lf|%lf|%lf|%lf|%lf|%lf",
                       &(tmp_3d_base.x1), &(tmp_3d_base.x2), &(tmp_3d_base.y1), &(tmp_3d_base.y2), &(tmp_3d_base.z1), &(tmp_3d_base.z2));
                sscanf(_DynIn[_k].ColVal[i],"%lf|%lf|%lf|%lf|%lf|%lf",
                       &(tmp_3d_new.x1), &(tmp_3d_new.x2), &(tmp_3d_new.y1), &(tmp_3d_new.y2), &(tmp_3d_new.z1), &(tmp_3d_new.z2));
                //find which coordinate between tmp_3d_base and tmp_3d_new is different and return the input new value  (e.g., _Inv = abs(x2-x1))
                if( FindDiffDC(tmp_3d_base, tmp_3d_new, _Inv ) != 0 )
                {
                    printf( "FindDiffDC() error! base_value=[%s], new_value=[%s]\n", _IBV, _DynIn[_k].ColVal[i] );
                    return -1;
                }
            }
            return 0;
        }
    }
    printf( "can't find _Inv: _IA=%s, _k=%d\n", _IA, _k );
    return -1;
}

/************************************************************************************************************************************************* 
 * Function: This function will find one sensitivity from the sensitivity matrix based on the maching information given by _OA and _IA 
 * _OA: input parameter indicating OutputAlias, 
 * _IA: input parameter indicating InputAlias, 
 * _sen_matx: input parameter indicating the sensitivity matrix, 
 * _one_sen: output parameter holding a sensitivity value
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int FindOneSen( char *_OA, char *_IA, char _sen_matx[MAXINPUTSNUM+1][MAXOUTPUTSNUM+1][128], double * _one_sen)
{
    int i=0,j=0,flag=0;
    
    for( i=1; i<=MAXINPUTSNUM; i++ )
    {
        if( strlen(_sen_matx[i][0]) == 0 )
            break;
        if( strcmp(_sen_matx[i][0], _IA) == 0 )
        {
            flag++;
            break;   
        }
    }

    for( j=1; j<=MAXOUTPUTSNUM; j++ )
    {
        if( strlen(_sen_matx[0][j]) == 0 )
            break;
        if( strcmp(_sen_matx[0][j], _OA ) == 0 )
        {
            flag++;
            break;
        }
    }

    if( flag == 2 )
    {
        *_one_sen = atof( _sen_matx[i][j]);
        return 0;
    }

    return -1;
}

/************************************************************************************************************************************************* 
 * Function: calculate the measures of single factor that can close the building fire performance gap 
 * _SenMatx: input parameter indicating the sensitivity matrix
 * _OA: input parameter indicating a output alias which should be registed in the sensitivity matrix 
 * _gap: input parameter indicating a building fire performance gap
 * _measures: output parameter indicating a series of optional measures of single factor that could  close the gap
 * Return: void
 *************************************************************************************************************************************************/
void CalMeasures(char _SenMatx[MAXINPUTSNUM+1][MAXOUTPUTSNUM+1][128], char *_OA, double _gap, char *_measures )
{
     int tmp_i=0, tmp_j=0;

     for( tmp_i=1; tmp_i<MAXINPUTSNUM; tmp_i++ )
     {
         if( strlen(_SenMatx[tmp_i][0]) == 0 )
         break;
         if( strlen(_measures) > 0 )
         strcat(_measures,"||");
         for( tmp_j=1; tmp_j<MAXOUTPUTSNUM; tmp_j++ )
         {

             if( strlen(_SenMatx[0][tmp_j]) == 0 )
                 break;
             if( strcmp( _SenMatx[0][tmp_j], _OA ) == 0 )
             {
                 char tmp_str[128];
                 double tmp_adjust = -_gap/atof(_SenMatx[tmp_i][tmp_j]);

	         memset( tmp_str, 0x0, sizeof(tmp_str) );
                 sprintf( tmp_str, "%s[%.4f]", _SenMatx[tmp_i][0], tmp_adjust);
                 strcat(_measures,tmp_str);
             }
         }
                        
     }
}

/************************************************************************************************************************************************* 
 * Function: this function is the core function of FirePM software. it uses dynamically changed input data from Dyn.txt to calculate the predictions by SMM and RSM.
 *    FlowChart:
 *    1. open a file with the name of _FPM_fn which will contain the predicted results (by default this file name is "FirePM.csv")
 *    2. for each output variable, 
 *       2.1 initialize RSM and SMT predictions
 *       2.2 calculate the RSM prediction by using the power curve fitting parameters
 *       2.3 calculate the SMT prediction by using the sensitivity matrix 
 *    3. save the predicted results to _FPM_fn and print them to stdout
 * _FPM_fn: input parameter indicating the file name of FirePM tool (FirepM.csv)
 * FDS_*: these variables starting with FDS_ are global variables whose values have been set before this function call of UpdateFPM()
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int UpdateFPM(char *_FPM_fn )
{
    int i=0,j=0,k=0, start=0;
    char tmp_base_name[MAXOUTPUTSNUM][128];
    FILE *fp=NULL; 

    if( findsize(_FPM_fn) > 0 )
        start = 1;
    
    fp = fopen(_FPM_fn,"a+");
    if( fp==NULL)
    {
        printf( "cannot open %s!\n", _FPM_fn );
        return -1;
    }

    memset( tmp_base_name, 0x0, sizeof( tmp_base_name ) );
    printf( "file size = %ld\n", ftell(fp) );
    //printf( "processing perfromance monitoring file : _FPM_fn=%s...\n", _FPM_fn );

    sprintf( FDS_OutputsRltSMT[0].ColName,"%s", FDS_DynIn[0].ColName );//initiallize the output data sequence with the input dynamic data;

    for( j=0; j<MAXOUTPUTSNUM;j++) //for each output variable
    {
        double tmp_colval_SMT[MAXLINENUM];
        double tmp_colval_RSM[MAXLINENUM];
        char InputAlias[MAXLINENUM][512];
        char InputNewValue[MAXLINENUM][512];

        memset( tmp_colval_SMT, 0x0, sizeof(tmp_colval_SMT));
        memset( tmp_colval_RSM, 0x0, sizeof(tmp_colval_RSM));
        memset( InputAlias, 0x0, sizeof(InputAlias));
        memset( InputNewValue, 0x0, sizeof(InputNewValue));


        if( strlen(FDS_OutputsVar[0].ColVal[j])==0 )
            break;
        
        sprintf( FDS_OutputsRltSMT[0].ColVal[j],"%s_SMT", FDS_OutputsVar[0].ColVal[j]);//initiallize table head, namely the first line
        sprintf( FDS_OutputsRltRSM[0].ColVal[j],"%s_RSM", FDS_OutputsVar[0].ColVal[j]);//initiallize table head, namely the first line
        sprintf( tmp_base_name[j],"%s_BAS", FDS_OutputsVar[0].ColVal[j]);//initiallize table head, namely the first line

        for( k=1; k<MAXLINENUM; k++ ) //k starts with 1 because the first line (k=0) in Dyn.txt  is head information
        {
           if( strlen(FDS_DynIn[k].ColName) == 0 )
               break;
           sprintf( FDS_OutputsRltSMT[k].ColName,"%s", FDS_DynIn[k].ColName );//initiallize the output data sequence with the input dynamic data;
           tmp_colval_SMT[k]=atof(FDS_OutputsVar[1].ColVal[j]); //initiallize the output data with the outputbase value
           tmp_colval_RSM[k]=atof(FDS_OutputsVar[1].ColVal[j]); //initiallize the output data with the outputbase value
           printf( "tmp_colval_SMT[%d]=%.2lf\n", k, tmp_colval_SMT[k] );
        }

        for( i=0; i<MAXINPUTSNUM; i++ )//for each input variable
        {
            double tmp_one_sen=0.00;
            if(strlen(FDS_InputsVar[0].ColVal[i])==0)
               break;
            // get one sensitivity from the sensitivity  matrix (SenMatx)
            FindOneSen(FDS_OutputsVar[0].ColVal[j], FDS_InputsVar[0].ColVal[i], SenMatx, &tmp_one_sen);
            for( k=1;k<MAXLINENUM;k++)
            {
                double tmp_input=0.00;
                double tmp_add=0.00;
                char tmp_str[128];
               
                memset( tmp_str, 0x0, sizeof(tmp_str));
                
                if( strlen(FDS_DynIn[k].ColName) == 0 )
                    break;
           
                //get one input new value
                if( FindOneINV(FDS_InputsVar[0].ColVal[i],FDS_InputsVar[1].ColVal[i],FDS_DynIn,k, &tmp_input) != 0 )
                {
                    printf( "FindOneINV() error !, InputAlias = [%s], input base value=[%s], k=[%d]\n", 
                            FDS_InputsVar[0].ColVal[i], FDS_InputsVar[1].ColVal[i], k );
                    return -1;
                }
                // deal with RSM
                if( i != 0 )
                {
                    strcat(InputAlias[k], "+" );
                    strcat(InputNewValue[k], "+" );
                }
                strcat(InputAlias[k],FDS_InputsVar[0].ColVal[i] );
                sprintf( tmp_str, "%lf", tmp_input );
                strcat(InputNewValue[k],tmp_str ); // prepare _IA and _INV for GetOnePvFromRSMRlt()

                // deal with SMT
                if( strstr(FDS_InputsVar[1].ColVal[i], "|") == NULL )
                    tmp_add = tmp_one_sen*(tmp_input-atof(FDS_InputsVar[1].ColVal[i])); //physcical variables
                else { //geometric variables
                    double tmp_d = 0.00;
                    struct ThreeDCoordinate tmp_3d_base, tmp_3d_new;
                    memset( &tmp_3d_base, 0x0, sizeof(tmp_3d_base));
                    memset( &tmp_3d_new, 0x0, sizeof(tmp_3d_new));
                    sscanf(FDS_DynIn[k].ColVal[i],"%lf|%lf|%lf|%lf|%lf|%lf",
                       &(tmp_3d_base.x1), &(tmp_3d_base.x2), &(tmp_3d_base.y1), &(tmp_3d_base.y2), &(tmp_3d_base.z1), &(tmp_3d_base.z2));
                    sscanf(FDS_InputsVar[1].ColVal[i],"%lf|%lf|%lf|%lf|%lf|%lf",
                       &(tmp_3d_new.x1), &(tmp_3d_new.x2), &(tmp_3d_new.y1), &(tmp_3d_new.y2), &(tmp_3d_new.z1), &(tmp_3d_new.z2));
                    // find the input base value from the 3d coordinates by comparing with the input new value
                    // note that the base and new parameters are inverted to get the input base value
                    if( FindDiffDC(tmp_3d_base, tmp_3d_new, &tmp_d) != 0 ) 
                    {
                        printf( "FindDiffDC() error! base_value=[%s], new_value=[%s]\n", FDS_DynIn[k].ColVal[i], FDS_InputsVar[1].ColVal[i] );
                        return -1;
                    }
                    tmp_add = tmp_one_sen*(tmp_input-tmp_d);
                }
                tmp_colval_SMT[k] += tmp_add; //sum the additions from sensitivity matrix 

            }
        }

        for( k=1; k<MAXLINENUM-1;k++) //dealing with RSM fields
        {
            if( strlen(trim(FDS_OutputsRltSMT[k].ColName,NULL)) == 0 )
                break;
/*
            if( strcmp(FDS_OutputsRltSMT[k].ColName,"BaseValue") == 0 )
                break;
*/
            sprintf( FDS_OutputsRltSMT[k].ColVal[j], "%.2lf", tmp_colval_SMT[k] ); //fill into SMT field  
            if( GetOnePvFromRSMRlt( InputAlias[k], InputNewValue[k],FDS_OutputsVar[0].ColVal[j],  FDS_RSMResults, &(tmp_colval_RSM[k]) ) != 0  )
            {
                printf( "GetOnePvFromRSMRlt() error! k=%d, j=%d, InputAlias=%s, InputNewValue=%s,OutputAlias=%s\n", k, j, InputAlias[k], InputNewValue[k], FDS_OutputsVar[0].ColVal[j] );
                return -1;
            }
            sprintf( FDS_OutputsRltRSM[k].ColVal[j], "%.2lf", tmp_colval_RSM[k] ); //fill into RSM field
        }

        //the last line shows the base value since here k points to the last line
        /*if( j== 0)  // only set the "BaseValue" one time
            sprintf( FDS_OutputsRltSMT[k].ColName, "%s", "BaseValue");
        sprintf( FDS_OutputsRltSMT[k].ColVal[j], "%s", FDS_OutputsVar[1].ColVal[j]);
        sprintf( FDS_OutputsRltRSM[k].ColVal[j], "%s", FDS_OutputsVar[1].ColVal[j]);
        */
        //printf( "k=%d,j=%d, basevalue=%s\n", k, j, FDS_OutputsRltSMT[k].ColVal[j] );
    }

    //printf( "start = %d\n", start );

    for( k=0; k<MAXLINENUM; k++ )//print to FirePM.csv and stdout
    {

        //printf( "k=%d, basevalue_0=%s, basevalue_1=%s\n", k, FDS_OutputsRltSMT[k].ColVal[0],FDS_OutputsRltSMT[k].ColVal[1] );
        if( strlen(FDS_OutputsRltSMT[k].ColName) == 0 )
            break;

        if( k>0 || start == 0 ) // if the fire is not empty (start=1), we don't print the first line which is the head information to the file
            fprintf(fp, "%s", FDS_OutputsRltSMT[k].ColName);
        printf("\n\t\t%10s", FDS_OutputsRltSMT[k].ColName);
        for( j=0; j<MAXOUTPUTSNUM; j++ )
        {
            if( strlen(FDS_OutputsRltSMT[0].ColVal[j])==0 ) // if the name of a column is null, namely this is the end of output variables 
                break;
            if( k>0 && ( fabs(atof(FDS_OutputsRltSMT[k].ColVal[j])/atof(FDS_OutputsVar[1].ColVal[j])-1)>ALARMRATIO || fabs(atof(FDS_OutputsRltRSM[k].ColVal[j])/atof(FDS_OutputsVar[1].ColVal[j])-1)>ALARMRATIO ) )
            { // if the performance gap over the base value is greater than ALARMRATIO, print "*"  
                //printf( "Rlt=%s, base=%s, real SMT ratio = %lf, real RSM ratio = %lf, alarm ratio = %lf \n", (FDS_OutputsRltSMT[k].ColVal[j]),(FDS_OutputsVar[1].ColVal[j]),fabs(atof(FDS_OutputsRltSMT[k].ColVal[j])/atof(FDS_OutputsVar[1].ColVal[j])-1),fabs(atof(FDS_OutputsRltRSM[k].ColVal[j])/atof(FDS_OutputsVar[1].ColVal[j])-1), ALARMRATIO );
                fprintf(fp, ",%s", FDS_OutputsVar[1].ColVal[j] );
                printf( "\t%12s*", FDS_OutputsVar[1].ColVal[j] );

                fprintf(fp, ",%s", FDS_OutputsRltSMT[k].ColVal[j] );
                if( fabs(atof(FDS_OutputsRltSMT[k].ColVal[j])/atof(FDS_OutputsVar[1].ColVal[j])-1)>ALARMRATIO )
                {
                    char tmp_measures[MAXSTRINGSIZE];
                    double tmp_gap = atof(FDS_OutputsRltSMT[k].ColVal[j])-atof(FDS_OutputsVar[1].ColVal[j]);

                    memset( tmp_measures, 0x0, sizeof(tmp_measures) );
                    CalMeasures(SenMatx, FDS_OutputsVar[0].ColVal[j], tmp_gap, tmp_measures );

                    printf( "\t%12s*", FDS_OutputsRltSMT[k].ColVal[j] );
                    printf( "\t%s", tmp_measures );
                    fprintf( fp, ",%s", tmp_measures );
                }
                else 
                {
                    printf( "\t%12s", FDS_OutputsRltSMT[k].ColVal[j] );
                    printf( "\t%s", "    ");
                    fprintf( fp, "," );
                }

                fprintf(fp, ",%s", FDS_OutputsRltRSM[k].ColVal[j] );
                if( fabs(atof(FDS_OutputsRltRSM[k].ColVal[j])/atof(FDS_OutputsVar[1].ColVal[j])-1)>ALARMRATIO )
                    printf( "\t%12s*", FDS_OutputsRltRSM[k].ColVal[j] );
                else
                    printf( "\t%12s", FDS_OutputsRltRSM[k].ColVal[j] );

            }
            else
            {
                if( k==0 )    
                {
                     // if the file is empty(start==0) we will print the first line.  if the file is not empty (start=1), we don't print the first line which is the head information to the file
                    if( start == 0 ) 
                    {
                        fprintf(fp, ",%s", tmp_base_name[j] );
                        fprintf(fp, ",%s", FDS_OutputsRltSMT[k].ColVal[j] );
                        fprintf(fp, ",%s_MEA", FDS_OutputsRltSMT[k].ColVal[j] );
                        fprintf(fp, ",%s", FDS_OutputsRltRSM[k].ColVal[j] );
                    }
                    printf( "\t%12s", tmp_base_name[j] );
/*
                    if( start == 0 ) // if the file is not empty (start=1), we don't print the first line which is the head information to the file
                        fprintf(fp, ",%s", FDS_OutputsRltSMT[k].ColVal[j] );
*/
                    printf( "\t%12s", FDS_OutputsRltSMT[k].ColVal[j] );
                    printf( "\t%s_MEA", FDS_OutputsRltSMT[k].ColVal[j] );
/*
                    if( start == 0 ) // if the file is not empty (start=1), we don't print the first line which is the head information to the file
                        fprintf(fp, ",%s", FDS_OutputsRltRSM[k].ColVal[j] );
*/
                    printf( "\t%12s", FDS_OutputsRltRSM[k].ColVal[j] );
                } else {
                    fprintf(fp, ",%s", FDS_OutputsVar[1].ColVal[j] );
                    printf( "\t%12s", FDS_OutputsVar[1].ColVal[j] );
                    fprintf(fp, ",%s", FDS_OutputsRltSMT[k].ColVal[j] );
                    printf( "\t%12s", FDS_OutputsRltSMT[k].ColVal[j] );
                    fprintf(fp, "," );
                    printf( "\t%12s", "            ");
                    fprintf(fp, ",%s", FDS_OutputsRltRSM[k].ColVal[j] );
                    printf( "\t%12s", FDS_OutputsRltRSM[k].ColVal[j] );
                }
            }
        }
        if( k>0 || start == 0 ) // if the file is not empty (start=1), we don't print the first line which is the head information to the file
            fprintf(fp,"\n");
        printf("\n");
    }

    fclose(fp);
    return 0;
}

// read in power curve fitting parameters from a file (_RSMRlt_fn) and save to _RSMRlt
int readinRSMRlt( char *_RSMRlt_fn, struct RSMResults *_RSMRlt )
{
    int i=0;
    char Info[MAXSTRINGSIZE];
    FILE *fp = fopen( _RSMRlt_fn, "r" );
    if ( fp == NULL )
    {
       printf( "fopen() error, _smt_fn=[%s]\n",  _RSMRlt_fn);
       return -1;
    }
    
    memset(Info, '\0', sizeof (Info));

    while ( fgets( Info, sizeof(Info), fp) != NULL )
    {
        sscanf( Info, "%[^,],%[^,],%lf,%lf,%lf,%lf,%lf,%[^\n]", _RSMRlt[i].OutputAlias,_RSMRlt[i].InputAlias,&(_RSMRlt[i].a),&(_RSMRlt[i].b),&(_RSMRlt[i].r_sqr),&(_RSMRlt[i].s_sqr),&(_RSMRlt[i].OutputBaseValue), _RSMRlt[i].comment );
        
        i++;
        if( i>(MAXINPUTSNUM+1)*(MAXOUTPUTSNUM+1) )
            break;
    }

    i=0;
    do{ // output the information read from _RSMRlt_fn  to stdout 
        char tmp_str[MAXSTRINGSIZE];
        memset(tmp_str, 0x0, sizeof(tmp_str));
        sprintf( tmp_str, "%s,%s,%.2f,%.2f,%.6f,%.6f,%.2f\n", _RSMRlt[i].OutputAlias, _RSMRlt[i].InputAlias, _RSMRlt[i].a, _RSMRlt[i].b, _RSMRlt[i].r_sqr, _RSMRlt[i].s_sqr, _RSMRlt[i].OutputBaseValue );
        printf( tmp_str );
        i++;
    }while (strlen(trim(_RSMRlt[i].OutputAlias,NULL)) !=0 );

    fclose(fp);
    return 0;
}

//read in sensitivity matrix from _smt_fn and save to _sen_matx 
int readinSMT( char *_smt_fn, char _sen_matx[MAXINPUTSNUM+1][MAXOUTPUTSNUM+1][128] )
{
    int i=0,j=0;
    char Info[MAXSTRINGSIZE];
    FILE *fp = fopen( _smt_fn, "r" );
    if ( fp == NULL )
    {
       printf( "fopen() error, _smt_fn=[%s]\n",  _smt_fn );
       return -1;
    }

    printf( "precessing _smt_fn=[%s]...\n",  _smt_fn );

    memset(Info, '\0', sizeof (Info));

    while ( fgets( Info, sizeof(Info), fp) != NULL )
    {
        char *token=NULL;
        token = strtok(Info, "," );
        printf( "token = [%s]\n", token );       
        j=0;
        while ( token != NULL )
        {
           if( strlen(token) == 0 )
               break;
           sprintf( _sen_matx[i][j], "%s", token );   
           token = strtok(NULL, "," );
           j++;
           if( j>MAXOUTPUTSNUM )
               break;
           printf( "inner while: token = [%s], i=%d, j=%d \n", token, i, j );       
        }
        i++;
        if( i>MAXINPUTSNUM )
            break;
    }

    fclose(fp);

    for( i=0; i<=MAXINPUTSNUM; i++ )
    {// output the sensitivity matrix to stdout
        if( strlen(trim(_sen_matx[i][0],NULL)) == 0 )   
            break;
        for(j=0; j<=MAXOUTPUTSNUM; j++ )
        {
            if( strlen(trim(_sen_matx[i][j],NULL)) != 0 )   
                printf( " sen[%d][%d]= %s,", i,j,_sen_matx[i][j] );
        }
        printf( "\n" );
    }
      
    return 0;
}

// read in the dynamically changed input data (Dyn.txt ) and save to _DI
int readinDyn(char *_Dyn_fn, struct VarInCol *_DI ) 
{
    int i=0,j=0;
    char Info[MAXSTRINGSIZE];
    FILE *fp = fopen( _Dyn_fn, "r" );
    if ( fp == NULL )
    {
       printf( "fopen() error, _smt_fn=[%s]\n",  _Dyn_fn);
       return -1;
    }

    memset(Info, '\0', sizeof (Info));

    printf( "precess dynamically changed input data: _Dyn_fn=[%s]...\n", _Dyn_fn );
    if ( fgets( Info, sizeof(Info), fp) == NULL ) // we don't use the first line which is only explanatory informaiton 
    {
        // try close the file and reopen and reread, because another process is updating the file of _Dyn_fn
        fclose(fp);
        sleep(0.5); //if the first time fails, then sleep half second and reopen  the file since this file maybe being written by another process 
        fp=fopen(_Dyn_fn,"r");
        if ( fp == NULL )
        {
            printf( "fopen() error, _smt_fn=[%s]\n",  _Dyn_fn);
            return -1;
        }
        if( fgets(Info, sizeof(Info), fp) == NULL )
        {
            printf( "fgets() error! empty file: _Dyn_fn=%s\n", _Dyn_fn );
            fclose(fp);
            return -1;
        }
    }

    while ( fgets( Info, sizeof(Info), fp) != NULL )
    {
        char *token=NULL;
        token = strtok(Info, "," );
        j=0;
        while ( token != NULL )
        {
           if( j ==0 )
               sprintf( _DI[i].ColName, "%s", token );
           else {
               sprintf( _DI[i].ColVal[j-1], "%s", token);
           }
           printf( "token = %s, i=%d, j=%d\n", token, i, j );
           token = strtok(NULL, "," );
           j++;
           if( j>MAXINPUTSNUM )
               break;
        }
        i++;
        if( i>MAXLINENUM)
            break;
    }
    fclose(fp);

    for( i=0;i<MAXLINENUM; i++ )
    {//output the Dyn.txt to stdout
        if( i>0 )
        {
            if(strlen(trim(_DI[i].ColName, NULL))==0)
                break;
        }
        for( j=0; j<=MAXINPUTSNUM; j++ )
        {
            if( j==0 )
                printf( "%s,", _DI[i].ColName );
            else
            {
                if( strlen(trim(_DI[i].ColVal[j-1], NULL) )== 0 )
                    break;
                printf( "%s,", trim(_DI[i].ColVal[j-1],NULL));
            }
        }
        printf( "\n" );
    }

    printf( "after precessing dynamically changed input data: _Dyn_fn=[%s]\n", _Dyn_fn );
    return 0;
}

int main( int argc, char ** argv )
{
    char *tmp_ret=NULL;
    time_t tmp_t1_SMT=0, tmp_t2_SMT=0;
    time_t tmp_t1_Dyn=0, tmp_t2_Dyn=0;
    time_t tmp_t1_RSMRlt=0, tmp_t2_RSMRlt=0;
    
    if ( argc != 2 )
    {
        int i=0;
        for ( i=0; i<argc; i++ )
           printf( "%s\n", argv[i] );
        printf( "only one argument is needed, you have [%d] arguments\n" , argc);
        return -1;
    }
    memset( FDS_SmInfo, '\0', sizeof(FDS_SmInfo));
    memset( SenMatx, '\0', sizeof(SenMatx));
    memset( FDS_InputsVar, '\0', sizeof(FDS_InputsVar));
    memset( FDS_OutputsVar, '\0', sizeof(FDS_OutputsVar));
    memset( FDS_OutputsRltSMT, '\0', sizeof(FDS_OutputsRltSMT));
    memset( FDS_DynIn, '\0', sizeof(FDS_DynIn));

    if ( readin(argv[1], FDS_SmInfo) != 0 ){ 
        printf( "read SMInfo to structure error!\n" );
        return -1;
    }
    if( GetVIC(FDS_SmInfo, FDS_InputsVar) != 0 )
    {
        printf( "GetVIC() error!\n" );
        return -1;
    }
    if( GetVOC(FDS_SmInfo, FDS_OutputsVar) != 0 )
    {
        printf( "GetVOC() error!\n" );
        return -1;
    }

 while(1)
 {
    sleep(1);
    tmp_t2_SMT=getFileModifiedTime("SMT.csv");
    if( tmp_t2_SMT !=tmp_t1_SMT )
    {
        memset( SenMatx, '\0', sizeof(SenMatx));
        if( readinSMT("SMT.csv", SenMatx) != 0 )
        {
            printf( "readinSMT() error!\n" );
            return -1;
        }
        tmp_t1_SMT=tmp_t2_SMT;
    } else {
       printf( "no modification made to SMT.csv since %s.\n", ctime(&tmp_t1_SMT) ) ;
    }

    tmp_t2_RSMRlt=getFileModifiedTime("RSMRlt.csv");
    if( tmp_t2_RSMRlt!=tmp_t1_RSMRlt)
    {
        memset( FDS_RSMResults, '\0', sizeof(FDS_RSMResults));
        if( readinRSMRlt("RSMRlt.csv", FDS_RSMResults) != 0 )
        {
            printf( "readinSMT() error!\n" );
            return -1;
        }
        tmp_t1_RSMRlt=tmp_t2_RSMRlt;
    } else {
       printf( "no modification made to SMT.csv since %s.\n", ctime(&tmp_t1_RSMRlt) ) ;
    }

    tmp_t2_Dyn = getFileModifiedTime("Dyn.txt");
    if( tmp_t2_Dyn != tmp_t1_Dyn )
    {
        memset( FDS_OutputsRltSMT, '\0', sizeof(FDS_OutputsRltSMT));
        memset( FDS_OutputsRltRSM, '\0', sizeof(FDS_OutputsRltRSM));
        memset( FDS_DynIn, '\0', sizeof(FDS_DynIn));
        if( readinDyn("Dyn.txt", FDS_DynIn ) != 0 )
        {
            printf( "readinDyn() error!\n" );
            return -1;
        }
        if( UpdateFPM("FirePM.csv") != 0 )
        {
            printf( "UpdateFPM() error !\n" );
            return -1;
        }
        tmp_t1_Dyn=tmp_t2_Dyn;
    } else{
       printf( "no modification made to Dyn.txt since %s.\n", ctime(&tmp_t1_Dyn)) ; 
    }

  }

  return 0;
}
