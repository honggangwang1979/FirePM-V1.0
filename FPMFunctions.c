/****************************************************************************************************************************************************
 *  Author: Honggang Wang and Professor Dembsey, the Department of Fire Protection Engineering in WPI
 *  Copyright: This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation with the author information being cited
 *
 *  Discription: this file includes commonly used  functions in this FirePM software project
 *
 ***************************************************************************************************************************************************/
#include "FirePM.h"
#include <tgmath.h>

const char s[2]=",";
// trim the left side
char *ltrim(char *str, const char *seps)
{
    size_t totrim;
    if (seps == NULL) {
        seps = "\t\n\v\f\r ";
    }
    totrim = strspn(str, seps);
    if (totrim > 0) {
        size_t len = strlen(str);
        if (totrim == len) {
            str[0] = '\0';
        }
        else {
            memmove(str, str + totrim, len + 1 - totrim);
        }
    }
    return str;
}

// trim the right side
char *rtrim(char *str, const char *seps)
{
    int i;
    if (seps == NULL) {
        seps = "\t\n\v\f\r ";
    }
    i = strlen(str) - 1;
    while (i >= 0 && strchr(seps, str[i]) != NULL) {
        str[i] = '\0';
        i--;
    }
    return str;
}

// trim both sides
char *trim(char *str, const char *seps)
{
    return ltrim(rtrim(str, seps), seps);
}

// return the last modified time of file denoed by path
time_t getFileModifiedTime(char *path) 
{
    struct stat attr;
    stat(path, &attr);
    //printf("Last modified time: %s\n", ctime(&(attr.st_mtime)));
    return attr.st_mtime;
}

// return a random double value between 0 and 1.1
double rand_double() 
{
   return rand()/(double)RAND_MAX;
}

// return a random double value between x and y
double rand_double2(double x, double y) 
{
   return (y - x)*rand_double() + x;
}

// return a random int value between a and b 
int rand_int(int a, int b) 
{
   return (int)((b - a + 1)*rand_double()) + a;
}

/************************************************************************************************************************************************* 
 * Function: linear regression analysis -- curve fitting
 * _x,_y: input parameter indicating a group of experimental data, _x for independent variable values, _y for dependent variable values
 * _n: input parameter indicating the length of _x and _y
 * _a: output parameter indicating the a value in curve fitting function y=a + bx
 * _b: output parameter indicating the b value in curve fitting function y=a + bx
 * _r_sqr: output parameter indicating the r-squared value (coefficient of determination, or correlation coefficient) of the curve fitting function y=a+bx 
 * _s_sqr: output parameter indicating the s-squared value ( the variance of errors between the actual value and the fitted value) of the curve fitting function y=a+bx
 * Return:  void
 *************************************************************************************************************************************************/
void LinearFit( double *_x, double *_y, int _n, double *_a, double *_b, double *_r_sqr, double *_s_sqr )
{
    int i;
    double sumX=0.0, sumX2=0.0, sumY=0.0, sumY2=0.0, sumXY=0.0, a=0.0, b=0.0;
    double SSxx=0.0, SSyy=0.0, SSxy=0.0;

    /* Calculating Required Sum */
    for(i=0;i<_n;i++)
    {
        sumX = sumX + _x[i]; // sum _x
        sumX2 = sumX2 + _x[i]*_x[i];//sum  _x^2
        sumY = sumY + _y[i]; // sum _y
        sumY2 = sumY2 + _y[i]*_y[i]; //sum _y^2
        sumXY = sumXY + _x[i]*_y[i]; //sum _x*_y
        printf( "x[%d]=%f,y[%d]=%f\n", i,_x[i], i,_y[i]);
    }

    SSxx = sumX2-sumX*sumX/_n;
    SSyy = sumY2-sumY*sumY/_n;
    SSxy = sumXY-sumX*sumY/_n;

    /* Calculating a and b */
    b = (_n*sumXY-sumX*sumY)/(_n*sumX2-sumX*sumX);
    a = (sumY - b*sumX)/_n;
   
    /* Displaying value of a and b */
    printf("\nMethod 1: Equation of best fit is: y = %0.2f + %0.2fx",a,b);
    printf("\nMethod 2: Equation of best fit is: y = %0.2f + %0.2fx\n",(sumY-(SSxy/SSxx)*sumX)/_n,SSxy/SSxx);
    *_a = a;
    *_b = b;
    *_r_sqr = (SSxy*SSxy)/(SSxx*SSyy);
    if( _n > 2 )
        *_s_sqr = (SSyy-b*SSxy)/(_n-2);
    printf( "_n= %d, SSxy=%f, SSxx=%f, SSyy=%f, _r_sqr=%f, _s_sqr=%f\n", _n, SSxy, SSxx, SSyy, *_r_sqr, *_s_sqr);
    return;
}

// power function curve fitting by using the linear fitting function: Y=AX^B ==> y=a + bx provided x=log(X) and y=log(Y)
void PowerFit( double *_X, double *_Y, int _n, double *_A, double *_B, double *_R_sqr, double *_S_sqr )
{
    int i=0;
    double x[_n], y[_n], a=0.0, b=0.0, r_sqr, s_sqr;

    memset( x, 0x0, sizeof(x));
    memset( y, 0x0, sizeof(y));

    for( i=0; i<_n; i++ )
    {
        x[i]=log(_X[i]);
        y[i]=log(_Y[i]);
        printf( "_X[%d]=%f,_Y[%d]=%f\n", i,_X[i], i,_Y[i]);
    }
    LinearFit(x,y,_n,&a,&b, &r_sqr, &s_sqr);
    *_A = exp(a);
    *_B = b;
    *_R_sqr = r_sqr;
    *_S_sqr = s_sqr;
    
    return;
}


/************************************************************************************************************************************************* 
 * Function: generate a VarOutCol struct list (the length of the list is only 2) holding the output alias name and the output basevalue
 * _si: input parameter indicating the content of the input configuration file 
 * _voc: output parameter indicating a VarOutCol structure 
 * Return: 0: success
 *         -1: failure 
 *************************************************************************************************************************************************/
int GetVOC(struct SMInfo *_si, struct VarOutCol *_voc) 
{
    int i=0,j=0;

    if( _si == NULL || _voc == NULL )
    {
        printf( "_si or _vic is null!\n" );
        return -1;
    }

    sprintf(_voc[0].ColName, "OutputAlias" );
    sprintf(_voc[1].ColName, "OutputBaseValue" );

    for( i=0; i<MAXLINENUM; i++ ) //for each record in _si
    {
        int flag=0;
        if( strlen(_si[i].VarType) == 0 ) 
            break;//since most of the time the _si is not full, we don't want to waste the cpu time to search the empty records
        if( _si[i].VarType[0] != 'O' )
            continue; //only the Output part of the configuration file is addressed
        for( j=0;j<MAXOUTPUTSNUM; j++ ) // for each output variable 
        {
            if( strlen(_voc[0].ColVal[j]) == 0 )
                break;//since most of the time the ColVal is not full, we don't want to waste the cpu time to search the empty records
            if( strcmp(_si[i].Alias, _voc[0].ColVal[j]) == 0 )
            {
                flag = 1; //means the output Alias has already been added into the ColVal
                break;
            }

        }
        //printf( "before flag = %d, i=%d,j=%d, _si.Alias=%s\n", flag, i, j, _si[i].Alias );
        if( flag == 0 )// not yet included in the outputalias list
        {
            double tmp_base_value=0.0;
            sprintf(_voc[0].ColVal[j], "%s", _si[i].Alias );
            //printf( "1 in flag = 0, i=%d,j=%d, _si.Alias=%s\n", i, j, _si[i].Alias );
            if( getOutputBaseValue( basefile, _si[i], &tmp_base_value ) != 0 )
            {//get output base value from the simulation result of basefile matching _si[i]
                printf( "getOutputBaseValue() error, basefile=[%s], i=%d!\n", basefile, i ); 
                return -1;
            }
            sprintf(_voc[1].ColVal[j], "%.2lf", tmp_base_value );
            //printf( "2 in flag = 0, i=%d,j=%d, _si.Alias=%s\n", i, j, _si[i].Alias );
        }
    }

    //printf( "after for : i=%d,j=%d, _si.Alias=%s\n", i, j, _si[i].Alias );
    return 0;
}

/************************************************************************************************************************************************* 
 * Function: generate a VarinCol struct list (the length of the list is only 2) holding the input alias name and the input basevalue
 * _si: input parameter indicating the content of the input configuration file 
 * _voc: output parameter indicating a VarOutCol structure 
 * Return: 0: success
 *         -1: failure 
 *************************************************************************************************************************************************/
int GetVIC( struct SMInfo * _si, struct VarInCol *_vic )
{
    int i=0,j=0;

    if( _si == NULL || _vic == NULL )
    {
        printf( "_si or _vic is null!\n" );
        return -1;
    }

    sprintf(_vic[0].ColName, "InputAlias" ); 
    sprintf(_vic[1].ColName, "InputBaseValue" ); 

    for( i=0; i<MAXLINENUM; i++ )
    {
        int flag=0;
        if( strlen(_si[i].VarType) == 0 )
            break;
        if( _si[i].VarType[0] != 'I' )
            continue;
        for( j=0;j<MAXINPUTSNUM; j++ )
        {
            if( strlen(_vic[0].ColVal[j]) == 0 )
                break;
            if( strcmp(_si[i].Alias, _vic[0].ColVal[j]) == 0 )
            {
                flag = 1;
                break;
            }
            
        }
        if( flag == 0 )// not yet included in the inputalias list
        {
            sprintf(_vic[0].ColVal[j], "%s", _si[i].Alias );
            sprintf(_vic[1].ColVal[j], "%s", _si[i].BaseValue);
        }
        
    }
    return 0;
}


/************************************************************************************************************************************************* 
 * Function: this function is used to compare if two strings include same items seperated by _sep evenif the sequence of these items in the two strens are different. For example, "aaa+bbb+ccc" is same to "bbb+aaa+ccc".
 * _csv: input parameter indicating a csv file name
 * _si: input parameter indicating the input/output information structure array
 * Return: 0: Yes, the file _csv is what we need
 *         -1: No, we don't need the file _csv
 *************************************************************************************************************************************************/
int CmpStrs( char *_str1, char *_str2, char *_sep )
{
    int i=0;
    char tmp_str1[MAXINPUTSNUM][128];
    char tmp_str2[MAXINPUTSNUM][128];
    struct VarInCol tmp_Cmp[2];

    if( strstr( _str1, _sep ) == NULL || strstr( _str2, _sep ) == NULL )
        return -1;

    printf( "in CmpStrs 1, _str1 = %s, _str2= %s\n", _str1, _str2 );

    memset( tmp_str1, 0x0, sizeof(tmp_str1));
    memset( tmp_str2, 0x0, sizeof(tmp_str2));
    memset( tmp_Cmp, 0x0, sizeof(tmp_Cmp));

    if( GetVICFromStrs( _str1, _str2, tmp_Cmp ) == -1)
    {
        printf( "GetVICFromStrs() failed in CmpStrs(), _str1=%s, _str2=%s, _sep=%s\n", _str1, _str2, _sep );
        return -1;
    }
    for( i=0;i<MAXINPUTSNUM; i++)
    {
        int j=0;
        int flag=0;
        if( strlen(tmp_Cmp[0].ColVal[i]) == 0 )
            break;
        for( j=0; j<MAXINPUTSNUM; j++)
        {
            if( strlen(tmp_Cmp[1].ColVal[j]) == 0 )
                break;
            if( strcmp(trim(tmp_Cmp[0].ColVal[i],NULL),trim(tmp_Cmp[1].ColVal[j],NULL)) == 0 )
            {
                flag = 1;
                break;
            }
        }
        if( flag == 0 )
            return -1;
    }
    printf( "in CmpStrs 2, _str1 = %s, _str2= %s\n", _str1, _str2 );
    return 0;
}
/************************************************************************************************************************************************* 
 * Function: this function looks up the RSMRlt list to find a record matching _IA and _OA and return the parameters of a and b
 * Input parameters:
 *     _IA: InputAlias; _OA: OutputAlias, 
 *     _RSMRlt: a list including the power fitting curve parameters between single output and single input, finally this list  will also include entries between single output and all the inputs
 * Output parameters:
 *     _a: parameter A of the power fitting curve (y=Ax^B)
 *     _b: parameter B of the power fitting curve (y=Ax^B)
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int GetParFromRSMRlt(char *_IA, char *_OA, struct RSMResults *_RSMRlt, double *_a, double *_b )
{
    int i=0;

    if( _IA == NULL || _OA == NULL || _RSMRlt == NULL || _a == NULL || _b == NULL )
    {
        printf( "GetParFromRSMRlt() error: one of the parameter pointer is NULL, please check!\n" );
        return -1;
    }

    for( i=0; i<MAXLINENUM; i++ )
    {
        if( strlen(_RSMRlt[i].OutputAlias) == 0 )
            break;
        printf( "i=%d, OutputAlias=[%s], _OA = [%s], InputAlias=[%s], _IA=[%s]\n", i, _RSMRlt[i].OutputAlias, _OA, _RSMRlt[i].InputAlias, _IA );
        if( strcmp(_RSMRlt[i].OutputAlias,_OA) == 0 && (strcmp(_RSMRlt[i].InputAlias,_IA)==0 || CmpStrs(_RSMRlt[i].InputAlias, _IA,"+")==0 ))
        {
            *_a = _RSMRlt[i].a;
            *_b = _RSMRlt[i].b;
            printf( "*_a=%lf, *_b=%lf\n", *_a, *_b );
            return 0;
        }   
    } 

    printf( "Cannot find a record matching _IA=%s and _OA=%s in _RSMRlt!\n", _IA, _OA );
    return -1;
}

/************************************************************************************************************************************************* 
 * Function: Obtain a structure include the input variables' name and input new values 
 * _IAs: input parameter indicating a series of input alias seperated by "+"
 * _Ivs: input parameter indicating a series of input new values  seperate by "+"
 * _VIC: output parameter holding the information of _IAs and IVs
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int GetVICFromStrs(char *_IAs, char *_IVs, struct VarInCol *_VIC )
{
    int j=0;
    char tmp_IAs[1024];
    char tmp_IVs[1024];
    char *tk_1=NULL;
    char *tk_2=NULL;

    if( _IAs == NULL || _IVs == NULL || _VIC == NULL )
    {  
        printf( "GetVICFromStrs() error! at lest one of the paramters are NULL!" );
        return -1;
    }

    memset( tmp_IAs, 0x0, sizeof(tmp_IAs) );
    memset( tmp_IVs, 0x0, sizeof(tmp_IVs) );

    sprintf( tmp_IAs, "%s", _IAs );
    sprintf( tmp_IVs, "%s", _IVs );

    tk_1 = strtok(tmp_IAs, "+");
    if( tk_1 == NULL )
    {
        printf( "GetXYFromCMBnRSMRlt() error: strtok() error: tmp_IAs=%s\n", tmp_IAs );
        return -1;
    }
    j=0;
    sprintf(_VIC[0].ColName, "InputAlias" );
    while( tk_1 != NULL )
    {
        sprintf( _VIC[0].ColVal[j++], "%s", tk_1 );
        tk_1 = strtok(NULL, "+" );
    }
    tk_2 = strtok(tmp_IVs, "+");
    if( tk_2 == NULL )
    {
        printf( "GetXYFromCMBnRSMRlt() error: strtok() error: tmp_IAs=%s\n", tmp_IVs );
        return -1;
    }

    j=0;
    sprintf(_VIC[1].ColName, "InputNewValue" );
    while( tk_2 != NULL )
    {
        sprintf( _VIC[1].ColVal[j++], "%s", tk_2 );
               tk_2 = strtok(NULL, "+" );
    }
    return 0;
}

/************************************************************************************************************************************************* 
 * Function: calculate one predicted result using the RSM's power curve parameters
 * _IA: input parameter indicating several InputAlias seperated by "+"
 * _IV: input parameter indicating several Input new values seperated by "+"
 * _RSMRlt: input parameter holding the power curve fitting parameters
 * _one_pv: output parameter holding the calculated the predicted result 
 * Return: 0: Success
 *         -1: Failure
 *************************************************************************************************************************************************/
int GetOnePvFromRSMRlt(char *_IA, char *_IV, char *_OA, struct RSMResults *_RSMRlt, double *_one_pv )
{
 
    int tmp_j=0;
    double tmp_one_pv=1.0; 
    double tmp_a=0.0, tmp_b=0.0;
    struct VarInCol tmp_VIC[2];
                  
    memset( tmp_VIC, 0x0, sizeof(tmp_VIC) );

    if( GetVICFromStrs(_IA, _IV, tmp_VIC ) != 0 )//form tmp_VIC with _IA(input Alias seperated by "+") and _IV(input values seperated by "+")
    {
        printf( "CmpRSMnSMT() error : GetVICFromStrs() error: InputAlias=%s, InputNewValue=%s\n", _IA, _IV);
        return -1;
    }
    while( strlen(tmp_VIC[0].ColVal[tmp_j]) != 0 ) //X*=x[i]^b[i]
    {
        double tmp_input_value = atof(tmp_VIC[1].ColVal[tmp_j]);

        //get the power curve fitting parameters of a and b for one input variable and one output variable 
        if( GetParFromRSMRlt(tmp_VIC[0].ColVal[tmp_j], _OA, _RSMRlt, &tmp_a, &tmp_b ) == -1 ) 
        {
            printf( "GetParFromRSMRlt()error! tmp_j=%d, _IA=%s, _OA=%s\n", tmp_j, tmp_VIC[0].ColVal[tmp_j], _OA );
            return -1;
        }
        printf( "tmp_one_pv=%lf, tmp_input_value=%lf\n", tmp_one_pv, tmp_input_value );
        tmp_one_pv *=  pow(tmp_input_value,tmp_b);//X*=x[i]^b[i]
        tmp_j++;
    }
                   
    tmp_a = 0.0, tmp_b= 0.0; 
    printf( "before second GetParFromRSMRlt(): _one_pv=%lf\n", *_one_pv );
    //get the power curve fitting parameters of A and B for combined input variables and one output variable 
    if( GetParFromRSMRlt(_IA, _OA, _RSMRlt, &tmp_a, &tmp_b ) == -1 )
    {
        printf( "GetParFromRSMRlt()error! _IA=%s, _OA=%s\n", _IA, _OA );
        return -1;
    }
    // put the output value predicted from RSM to the CMB structure 
    *_one_pv = tmp_a*pow(tmp_one_pv,tmp_b);//Y=A*X^B
    printf( "_one_pv=%lf\n", *_one_pv );
    fflush(stdout);
    return 0;
}

//calculate the size of the file 
long int findsize(char file_name[]) 
{ 
    // opening the file in read mode 
    FILE* fp = fopen(file_name, "r"); 
           
    // checking if the file exist or not 
    if (fp == NULL) { 
        printf("File Not Found!\n"); 
        return -1; 
    } 
                                         
    fseek(fp, 0L, SEEK_END); 
    
    // calculating the size of the file 
    long int res = ftell(fp); 
    
    // closing the file 
    fclose(fp); 
    return res; 
} 

/************************************************************************************************************************************************* 
 * Function: compare two 3d coordinate structs and return the difference between the two different component. it is supposed that there is only one different component
   _Base_3DC: input parameter indicating the base 3D coordinates
 * _New_3DC: input parameter indicating the new 3D coordinates
 * _P_d:  output parameter holding the found difference of the two different components
 * Return: 0: success
 *         -1: Failure
 *************************************************************************************************************************************************/
int FindDiffDC(struct ThreeDCoordinate _Base_3DC, struct ThreeDCoordinate _New_3DC, double *_p_d)
{
    if( fabs( _Base_3DC.x1 - _New_3DC.x1 ) > ZERO )
    {
        *_p_d = fabs(_New_3DC.x1-_New_3DC.x2);
        return 0;
    }
    else if ( fabs( _Base_3DC.x2 - _New_3DC.x2 ) > ZERO )
    {
        *_p_d =  fabs(_New_3DC.x2-_New_3DC.x1);
        return 0;
    }
    else if ( fabs( _Base_3DC.y1 - _New_3DC.y1 ) > ZERO )
    {
        *_p_d =  fabs(_New_3DC.y1 - _New_3DC.y2);
        return 0;
    }
    else if ( fabs( _Base_3DC.y2 - _New_3DC.y2 ) > ZERO )
    {
        *_p_d =  fabs(_New_3DC.y2 - _New_3DC.y1);
        return 0;
    }
    else if ( fabs( _Base_3DC.z1 - _New_3DC.z1 ) > ZERO )
    {
        *_p_d = fabs(_New_3DC.z1 - _New_3DC.z2);
        return 0;
    }
    else if ( fabs( _Base_3DC.z2 - _New_3DC.z2 ) > ZERO )
    {
        *_p_d = fabs( _New_3DC.z2 - _New_3DC.z1 );
        return 0;
    }
    return -1;
}

/************************************************************************************************************************************************* 
 * Function: this function is used to get one data from buff which has a 'D' instead of '.' becsaue '.' is not allowed in the CHID of FDS files
 * buff: input parameter indicating string (CHID in the FDS file)
 * _sep: input parameter indicating the delimiter
 * _OneCd: output parameter indicating one value transformed from a string which has the 'D' replaced with '.' (e.g, 45D22->45.22)
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int get1Data( char *buff, const char *_sep, double *_OneCd)
{
   char tmp_buff[MAXSTRINGSIZE];
   char *token = NULL;

   memset( tmp_buff, 0x0, sizeof(tmp_buff));
   sprintf( tmp_buff, "%s", buff );

   token = strtok(tmp_buff, _sep);
   if( token != NULL )
   {
      char *tmp_pt = token;
      int i=0;
      
      for( i=0;i<strlen(tmp_pt);i++)
          if( tmp_pt[i]=='D')
              tmp_pt[i]='.';
       *_OneCd  = atof(token);
       return 0;
   }else{
       printf( "get1Data() error: tmp_buff=[%s], _sep=[%s]\n", tmp_buff, _sep );
       return -1;
   }
}

/************************************************************************************************************************************************* 
 * Function: transform a string of coordinates seperated by _sep (e.g, '|' ) into six coordinates defining an obstruction in FDS
 * buff: input parameter indicating string (CHID in the FDS file)
 * _sep: input parameter indicating the delimiter
 * _SixCd: output parameter indicating six value transformed from a string (e.g, "1.2|2.1|3.4|4.5|2.3|3.4" )
 * Return: 0: success
 *         -1: Failure
 *************************************************************************************************************************************************/
int get6Data(char *buff, const char * _sep, double *_SixCd)
{
   char tmp_buff[MAXSTRINGSIZE];
   char *token = NULL;
   int counter=0;

   memset( tmp_buff, 0x0, sizeof(tmp_buff));
   sprintf( tmp_buff, "%s", buff );

   token = strtok(tmp_buff, _sep);
   if( token == NULL )
   {
       printf( "get6Data() error: tmp_buff=[%s], _sep=[%s]\n", tmp_buff, _sep );
       return -1;      
   }
   _SixCd[counter] = atof(token);

   while( token != NULL )
   {
       counter++;
       if( counter == 6 )
           break;
       token = strtok(NULL,_sep);
       _SixCd[counter] = atof(token);
   }

   if ( counter != 6 )
   {
       printf( "get6Data() error: tmp_buff=[%s], _sep=[%s], counter=[%d]\n", tmp_buff, _sep, counter );
       return -1;
   } else  {
       return 0;
   }
}

/************************************************************************************************************************************************* 
 * Function: this function is used to read the input configuration file with the name of _fn into a struct array _si
 * _fn: input parameter indicating a input configuration file
 * _si: output parameter indicating the output information structure array
 * Return: 0: Yes, the file _csv is what we need
 *         -1: No, we don't need the file _csv
 *************************************************************************************************************************************************/
int readin( char * _fn , struct SMInfo * _si )
{
    char Info[MAXSTRINGSIZE];
    FILE *fp = NULL; 
    char *token=NULL;
    int i=0,j=0;

    fp= fopen(_fn,"r");
    if( fp == NULL)
    {
       printf( "fopen wrong!\n");
       printf( _fn);
       printf("\n");
       return -1;
    }
    memset(Info, '\0', sizeof (Info));
    
    while ( fgets( Info, sizeof(Info), fp) != NULL )
    {
          char t_name[100], t_value[100];
          printf( Info );
          printf("\n");
          trim(Info,NULL);
          if( Info[0] == '#' || strlen(Info)==0 ) continue;
          
          sscanf(Info,"%[^'=']=%s", t_name, t_value); 
    //      printf( "t_name = %s, t_value=%s\n", t_name, t_value );
          if ( strcmp(trim(t_name,NULL), "BaseFile") == 0 ){
              sprintf(basefile, "%s", t_value ); 
              continue;
          }

          token = strtok(Info, s);
          while( token != NULL ){
              char tmp_name[100], tmp_value[100];
              memset(tmp_name, '\0', sizeof(tmp_name));
              memset(tmp_value, '\0', sizeof(tmp_value));
     //         printf( "token = %s, size of token = %d\n", token, sizeof(*token) );
              
              sscanf(token,"%[^'=']=%s", tmp_name, tmp_value); 
      //        printf( "tmp_name = (%s), tmp_value=(%s)\n", tmp_name, tmp_value );

              if( strcmp(trim(tmp_name,NULL), "VarType") == 0 ){
                  sprintf(_si[i].VarType, "%s", trim(tmp_value,NULL));
                  printf( "_si[%d].VarType = %s\n", i, _si[i].VarType);
              }
              else if( strcmp(trim(tmp_name,NULL), "Alias") == 0 ){
                  sprintf(_si[i].Alias, "%s", trim(tmp_value,NULL));
                  printf( "_si[%d].Alias= %s\n", i,_si[i].Alias);
              }
              else if( strcmp(trim(tmp_name,NULL), "TargetName") == 0 ){
                  sprintf(_si[i].TargetName, "%s", trim(tmp_value,NULL));
                  printf( "_si[%d].Alias= %s\n", i,_si[i].TargetName);
              }
              else if( strcmp(trim(tmp_name,NULL), "FDS_ID") == 0 ){
                  sprintf(_si[i].FDS_ID, "%s", trim(tmp_value,NULL));
                  printf( "_si[%d].FDS_ID= %s\n", i,_si[i].FDS_ID);
              }
              else if( strcmp(trim(tmp_name,NULL), "FDS_VarName") == 0 ){
                  sprintf(_si[i].FileVarName, "%s", trim(tmp_value,NULL));
                  printf( "_si[%d].FileVarName= %s\n", i,_si[i].FileVarName);
              }
              else if( strcmp(trim(tmp_name,NULL), "CriticalValue") == 0 ){
                  sprintf(_si[i].CriticalValue, "%s", trim(tmp_value,NULL));
                  printf( "_si[%d].CriticalValue= %s\n", i,_si[i].CriticalValue);
              }
              else if( strcmp(trim(tmp_name,NULL), "BaseValue") == 0 ){
                  sprintf(_si[i].BaseValue, "%s", trim(tmp_value,NULL));
                  printf( "_si[%d].BaseValue= %s\n", i,_si[i].BaseValue);
              }
              else if( strcmp(trim(tmp_name,NULL), "LowerLimit") == 0 ){
                  sprintf(_si[i].LowerLimit, "%s", trim(tmp_value,NULL));
                  printf( "_si[%d].LowerLimit= %s\n", i,_si[i].LowerLimit);
              }
              else if( strcmp(trim(tmp_name,NULL), "UpperLimit") == 0 ){
                  sprintf(_si[i].UpperLimit, "%s", trim(tmp_value,NULL));
                  printf( "_si[%d].UpperLimit= %s\n", i,_si[i].UpperLimit);
              }
              else {
                  sprintf(_si[i].Divisions, "%s", trim(tmp_value,NULL));
                  printf( "_si[%d].Divisions= %s\n", i,_si[i].Divisions);
              }
              
              token = strtok(NULL,s);
          }

          if( strlen(_si[i].VarType) > 0 )
             i++;
      }

    fclose (fp);
    return 0; 
}

/************************************************************************************************************************************************* 
 * Function: find the column position of a variable in the _buff seperated by _sep which is, for example, the head line of a CSV file
 * _buff: input parameter indicating the head line of a csv file
 * _FileVarName: input parameter indicating a variable in FDS file
 * _sep: input parameter indicating a delimiter
 * Return: a count >= 0 meaning the column position of _FileVarName in _buff
 *         -1: cannot find _FileVarName in _buff
 *************************************************************************************************************************************************/
int getColumnPos( char * _buff, const char *_FileVarName, const char *_sep )
{
    
    int counter=0;
    char *token  = NULL;
    char tmp_buff[MAXSTRINGSIZE];

    memset( tmp_buff, 0x0, sizeof(tmp_buff) );
    sprintf( tmp_buff, "%s", _buff );

//printf( "1 in getColumnPos():_buff=%s, tmp_buff=%s, _FileVarName=%s, _sep=[%s]\n", _buff, tmp_buff, _FileVarName,_sep );
    token = strtok(tmp_buff, _sep);
    if( token != NULL )
    {
        if( strcmp( trim(token,"\""), _FileVarName ) == 0 )
            return 0;
    }else{
        printf( "buff[%s] doesn't include _sep[%s]!\n", tmp_buff, _sep ); 
        return -1;
    }

//printf( "2 in getColumnPos():_buff=%s, tmp_buff=%s, _FileVarName=%s, _sep=[%s]\n", _buff, tmp_buff, _FileVarName,_sep );
    //printf( " %s\n",token);

    while( token != NULL )
    {
        counter++;
        token = strtok(NULL, _sep);
        if( token == NULL )
            break;
        if( strcmp( trim(token,"\""), _FileVarName ) == 0 )
            return counter;
    }
    return -1; // not found
}

/************************************************************************************************************************************************* 
 * Function: if the critical value (_CV) is met during a FDS simulation, this function is used to calculte an interpolation value . for example, if _CV = 5m of visibility, _VV_1 = 7m, _VV_2 = 4m, _NV_1=100s, _NV_2=103s, then the interpolation of _NV is 102s(ASET).
 * _NV_1: input parameter indicating a target value corresponding to _VV_1 which is before _CV
 * _NV_2: input parameter indicating a target value corresponding to _VV_2 which is after _CV
 * _VV_1: input parameter indicating a critical variable value before the critical value (_CV) is met
 * _VV_2: input parameter indicating a critical variable value after the critical value (_CV) is met
 * _CV: input parameter indicating the critical value which is used to judge if a condition is formed (e.g, visibility of 5m for an untenable condition)
 * Return: a double value indicating the interpolation value
 *************************************************************************************************************************************************/
double CalMidVal( double _NV_1, double _NV_2, double _VV_1, double _VV_2, double _CV )
{
    double tmp_d=0.0;
    //(_NV_1 - tmp_d)/(_NV_1-_NV_2)=(_VV_1-_CV)/(_VV_1-_VV_2)
    tmp_d = _NV_1 -(_NV_1-_NV_2)*(_VV_1-_CV)/(_VV_1-_VV_2);
    return tmp_d;
}

/************************************************************************************************************************************************* 
 * Function: find the the value of one column (_column) in a string (_buff) which is, for example, a data line in a CSV file
 * _buff: input parameter indicating the data line of a csv file
 * _column: input parameter indicating the position of a variable in the headline of a CSV file
 * _sep: input parameter indicating a delimiter
 * _return_d: output parameter indicating the value at _column in _buff
 * Return: 0: success
 *         -1: Failure
 *************************************************************************************************************************************************/
int getValueByCol(char *_buff, const int _column, const char *_sep, double *_return_d)
{
    int counter=0;
    char *token  = NULL;
    char tmp_buff[MAXSTRINGSIZE];

    memset( tmp_buff, 0x0, sizeof(tmp_buff));
    sprintf( tmp_buff, "%s", _buff );

    token = strtok(tmp_buff, _sep);
    if( token != NULL )
    {
        if( _column == counter)
        {
            *_return_d = atof(token);
            return 0;
        }
    }else{
        printf( "buff[%s] doesn't include _sep[%s]!\n", _buff, _sep );
        return -1;
    }

    while( token != NULL )
    {
        counter++;
        token = strtok(NULL, _sep);
        if( token == NULL )
        {
            printf( " inside getValueByCol() error: _column[%d]=%s, tmp_buff=[%s], _buff=[%s]\n",counter,token, tmp_buff, _buff);
            return -1;
        }

        if( _column == counter)
        {
            *_return_d = atof(token);
            return 0;
        }
    } 
    printf( "_column is bigger than counter: counter = %d, _column = %d\n", counter, _column );
    return -1;
}

/************************************************************************************************************************************************* 
 * Function: calculate the output base value correspoding to the output information in _single_si
 *     Flowchat: 
 *	1. find the devc.csv or evac.csv corresponding to _base_fn in ./base
 *	2. make sure the output variable (on which the critical value is set ) in the _single_si also exists in the csv file
 *	3. _col is the position of the output variables in the csv file
 *	4. when the critical value of the output variable is met, put the corresponding value of the target variable into _return_d 
 *	5. this function only calculate one value of the target variable specified by TargetName in _single_si. repeated calls of this function can generate all the values of all the target variables  if each time the _single_si is a different 'O' type record.
 *
 * _base_fn: input parameter indicating the name of the baseline FDS file
 * _single_si: input parameter indicating the output variable information
 * _return_d: output parameter indicating an output base value found in the csv file of the baseline fds simulation
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int getOutputBaseValue( const char * _base_fn, const struct SMInfo _single_si, double * _return_d )
{
    DIR *tmp_dir = NULL;
    struct dirent *en = NULL;
    char tmp_base_dir[MAXSTRINGSIZE];
    char tmp_base_fn[MAXSTRINGSIZE];
    char tmp_devc_fn[MAXSTRINGSIZE];
    char tmp_evac_fn[MAXSTRINGSIZE];
    char *tmp_head = NULL;
    int flag=0; // check if the critical value is met in the file fp
    
    memset( tmp_base_dir, 0x0, sizeof(tmp_base_dir) );
    memset( tmp_base_fn, 0x0, sizeof(tmp_base_fn) );
    memset( tmp_devc_fn, 0x0, sizeof(tmp_devc_fn) );
    memset( tmp_evac_fn, 0x0, sizeof(tmp_evac_fn) );

    sprintf( tmp_base_fn, "%s", _base_fn );
    tmp_head = strstr(tmp_base_fn, ".fds" );
    *tmp_head = '\0'; // remove the ".fds" from the base file name

    if (getcwd(tmp_base_dir, sizeof(tmp_base_dir)) == NULL) {
        perror("getcwd() error");
        return -1;
    }
    strcat( tmp_base_dir, "/base" );

    tmp_dir = opendir(tmp_base_dir);
    if( tmp_dir == NULL )
    {
        printf( "opendir error: [%s]\n", tmp_base_dir );
        return -1;
    }

    printf("Current working dir in getOutputBaseValue(): %s\n", tmp_base_dir);

     while ((en = readdir(tmp_dir)) != NULL)
     {
         FILE *fp = NULL;
         FILE *tmp_fp = NULL;
         char tmp_whole_fn[MAXSTRINGSIZE];
         char buff_1[MAXSTRINGSIZE];
         char buff_2[MAXSTRINGSIZE];
         double tmp_first_value=0.00;
         double tmp_first_NV=0.00;
         double tmp_first_CV = atof(_single_si.CriticalValue);
         int tmp_first_direction = atoi(_single_si.Divisions);
         int column = 0, column_TN;

         memset( tmp_whole_fn, 0x0, sizeof(tmp_whole_fn));
         memset( buff_1, 0x0, sizeof(buff_1));
         memset( buff_2, 0x0, sizeof(buff_2));
         

         //if( strstr( en->d_name, ".csv" ) == NULL || (strstr(en->d_name,"devc") == NULL 
          //                                        && strstr(en->d_name, "evac")== NULL ) || strstr(en->d_name, tmp_base_fn )==NULL )
         if( strstr( en->d_name, ".csv" ) == NULL || (strstr(en->d_name,"devc") == NULL 
                                                  && strstr(en->d_name, "evac")== NULL ) )
         {
             printf( "in while before continue, en->d_name = [%s], tmp_base_fn=[%s]!\n", en->d_name, tmp_base_fn );
             continue;
         }

         
         sprintf(tmp_whole_fn, "%s/%s", tmp_base_dir, en->d_name ); 
         fp=fopen(tmp_whole_fn,"r");
         if( fp == NULL )
         {
             printf( "open file [%s]failed!\n",tmp_whole_fn);
             return -1;
         }

         printf( "in while 2, en->d_name = [%s], tmp_base_fn=[%s], tmp_whole_fn=[%s]!\n", en->d_name, tmp_base_fn, tmp_whole_fn );
         
         if( fgets(buff_1,sizeof(buff_1),fp) == NULL ) // we don't need the first line of the csv file
         {
             printf( "read first line of [%s] error\n", tmp_whole_fn); 
             fclose(fp);
             return -1;
         } 
         printf( "in while 3,buff_1=[%s]!\n", buff_1); 
         if( fgets(buff_1,sizeof(buff_1),fp) == NULL ) // we need the second line of the csv file which is the headline 
         {
             printf( "read second line of [%s] error\n", tmp_whole_fn); 
             fclose(fp);
             return -1;
         } else {
             if( strstr(buff_1, _single_si.FileVarName) == NULL ) // the file doesn't include the output variable, check next file
             {
                 printf( "the second line of csv file doesn't include [%s] \n", _single_si.FileVarName );
                 continue;
             }
         }

         printf( "in while 4,buff_1=[%s]!\n", buff_1); 

         flag = 1;

         column = getColumnPos(buff_1, _single_si.FileVarName,s); // seek the column position of critical output variable in buff_1
         if( column == -1 )
         {
             printf("output variable [%s] not found in [%s]!\n", _single_si.FileVarName, buff_1 );
             fclose(fp);
             return -1;
         }
         column_TN = getColumnPos(buff_1, _single_si.TargetName,s);// seek the column position of target variable in buff_1 
         if( column_TN == -1 )
         {
             printf("output variable [%s] not found in [%s]!\n", _single_si.TargetName, buff_1 );
             fclose(fp);
             return -1;
         }

           fgets(buff_1,sizeof(buff_1),fp);
           if( getValueByCol(buff_1, column,s,&tmp_first_value) != 0 ) // the the first value of the critical output variable
           {
               printf("getValueByCol() error, buff_1 = [%s], column = [%d], sep=[%s]!\n", buff_1, column, s );
               fclose(fp);
               return -1;
           }
           if( getValueByCol(buff_1, column_TN, s, &tmp_first_NV) != 0 ) // the the first value of the target variable
           {
               printf("getValueByCol() error, buff_1 = [%s], column = [1], sep=[%s]!\n", buff_1, s );
               fclose(fp);
               return -1;
           }
           // tmp_first_direction =1 means the critical variable should increase with time, otherwise it should decrease with time
           if( (tmp_first_direction == 1 && tmp_first_value > tmp_first_CV )|| ( tmp_first_direction == -1 && tmp_first_value < tmp_first_CV ))
           {
               *_return_d = tmp_first_value;
               fclose(fp);
               return 0;
           }
           do
           { // find the basevalue in the following line
              int tmp_direction = tmp_first_direction;
              double tmp_value_1 = tmp_first_value;
              double tmp_value_2 = 0.0;
              double tmp_CV= tmp_first_CV;
              double tmp_NV_1=tmp_first_NV;
              double tmp_NV_2=0.0;

              fgets(buff_2,sizeof(buff_2),fp);
              if( getValueByCol(buff_2, column, s, &tmp_value_2) != 0 )// the the second value of the critical output variable 
              {
                  printf("getValueByCol() error, buff_1 = [%s], column = [%d], sep=[%s]!\n", buff_2, column, s );
                  fclose(fp);
                  return -1;
              }

              if( getValueByCol(buff_2, column_TN, s, &tmp_NV_2) != 0 )// the the second value of the target output variable 
              {
                  printf("getValueByCol() error, buff_1 = [%s], column = [%d], sep=[%s]!\n", buff_2, column_TN, s );
                  fclose(fp);
                  return -1;
              }

              //printf( "1. in do-while before return, direction =[%d], tmp_value_1=[%lf], tmp_value_2=[%lf],tmp_NV_1=[%lf],tmp_NV_2=[%lf],tmp_CV=[%lf],*_return_d=[%lf]\n", tmp_direction, tmp_value_1, tmp_value_2, tmp_NV_1, tmp_NV_2, tmp_CV,*_return_d);
              if((tmp_direction == 1 && tmp_value_2 > tmp_CV )||
                     ( tmp_direction == -1 && tmp_value_2 < tmp_CV ))
              {
                     *_return_d  = CalMidVal(tmp_NV_1, tmp_NV_2, tmp_value_1, tmp_value_2, tmp_CV);
              printf( "2. in do-while before return, direction =[%d], tmp_value_1=[%lf], tmp_value_2=[%lf],tmp_NV_1=[%lf],tmp_NV_2=[%lf],tmp_CV=[%lf],*_return_d=[%lf]\n", tmp_direction, tmp_value_1, tmp_value_2, tmp_NV_1, tmp_NV_2, tmp_CV,*_return_d);
                     fclose(fp);
                     return 0;
              }

              tmp_first_value = tmp_value_2; // marching forward the two values
              tmp_first_NV= tmp_NV_2; // marching forward the two values
           }while((getc(fp))!=EOF);

           printf( "critical value [%s] cannot be met in csv [%s] !\n", _single_si.CriticalValue, _base_fn);
           fclose(fp);
           return -1;
     }
}


/************************************************************************************************************************************************* 
 * Function: create new dir for each distinct VarType of input type (VarType[0] == 'I' )
 * _gi: input parameter indicating  GenInfo structure list
 * _NewDir: output parameter holding the created new directories 
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int GenNewDir( struct GenInfo *_gi, char _NewDir[MAXNEWDIRNUM][MAXSTRINGSIZE] )
{
    int i=0;
    for( i=0; i<MAXFILENUM; i++ )
    {
        int k=0;
        int newdir_flag = 0; // initialize the newdir_flag 
        char tmp_dir_name[MAXSTRINGSIZE];
        DIR *tmp_dir=NULL;

        memset(tmp_dir_name, 0x0, sizeof(tmp_dir_name));

        if( _gi[i].VarType[0] != 'I' )
            break;

        if (getcwd(tmp_dir_name, sizeof(tmp_dir_name)) != NULL) {
            //printf("Current working dir: %s\n", tmp_dir_name);
        } else {
            perror("getcwd() error");
            return -1;
        }         
        strcat( tmp_dir_name, "/" );
        strcat( tmp_dir_name, _gi[i].VarType );

        for( k=0; k<MAXNEWDIRNUM; k++ )
        {
           if( strlen(_NewDir[k]) == 0 )
               break;
           if( strcmp(_NewDir[k], tmp_dir_name) == 0 )
           {
               newdir_flag = 1; // the dir name is already in the NewDir list
               break;
           }
        }
        if( newdir_flag != 1) // tmp_dir_name has not been registed in the NewDir list, add it
        {
           sprintf( _NewDir[k], "%s", tmp_dir_name ); 
        }

        tmp_dir = opendir(tmp_dir_name);
        if( tmp_dir ) {
            closedir(tmp_dir);
            continue;
        } else if (ENOENT == errno) 
        {
            mkdir( tmp_dir_name, 0777 );
        }

    }
    return 0;
}

/************************************************************************************************************************************************* 
 * Function: find the position of one coordinate being changed 
 * _MoveTo: input parameter which is a string of 6 target coordinates seperated by "|"
 * _MoveFrom: input parameter which is a string of 6 original coordinates seperated by "|"
 * _Pos: input/output parameter holding the position of the coordinate being chaged. -1 is the initial value. if _MoveTo and _MoveFrom are identical,   *_Pos remains -1
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int FindPosDiff( char * _MoveTo, char * _MoveFrom, int *_Pos )
{
    int i = 0;
    char tmp_MoveTo[1024];
    char tmp_MoveFrom[1024];
    double ToList[6], FromList[6];

    if( _Pos == NULL )
    {
        printf( "FindPosDiff() error, _Pos is NULL!\n" );
        return -1;
    }

    memset(tmp_MoveTo, 0x0, sizeof(tmp_MoveTo) );
    memset(tmp_MoveFrom, 0x0, sizeof(tmp_MoveFrom) );
    memset(ToList, 0x0, sizeof(ToList) );
    memset(FromList, 0x0, sizeof(FromList) );

    sprintf( tmp_MoveTo, "%s", _MoveTo );
    sprintf( tmp_MoveFrom, "%s", _MoveFrom);

    get6Data(tmp_MoveTo, ",", ToList );
    get6Data(tmp_MoveFrom, ",", FromList );
    
    for( i=0;i<6;i++)
    {
        if( fabs(ToList[i] - FromList[i] ) > ZERO )
        {
            *_Pos=i;
            return 0;
        }
    }

    return -1;

}

/************************************************************************************************************************************************* 
 * Function: this function is used to replace specific fileds with _MoveTo 
 *       flowchart:
 *       1. if the variable is a coordinate variable then the basevalue and new value should have "|". we seperate the line (_Info) into 3 sections of which the second one include the _FileVarName and the _BaseValue seperated by "=". we replace the second section with the new value  (_MoveTo) and put the updated three sections back to _Info. The transformation of string coordinates to double type is needed.
 *       2. if the variable is a physical variable, the process is much easier.
 * _Info: input/output parameter indicating a line in a FDS file in which the _baseValue needs to be replaced by _MoveTo 
 * _VarType: input parameter indicating variable type of _FileVarName
 * _FDS_ID: input parameter indicating the ID where the _FileVarName is in the FDS file
 * _FileVarName: input parameter indicating the variable name whose basevalue needs to be replased with _MoveTo
 * _MoveTo: input parameter indicating the new value of _fileVarName 
 * _Pos: output parameter indicating which component is replaced if the FileVarName is a coordinate variable whose values are seperated by "|"
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int StrRpl( char * _Info, char * _VarType, char * _FDS_ID, char * _FileVarName, char * _BaseValue, char * _MoveTo, int *_Pos)
{
    int i=0;
    char *tmp_head=NULL;
    char *tmp_tail=NULL;
    char tmp_first[MAXSTRINGSIZE], tmp_second[MAXSTRINGSIZE], tmp_third[MAXSTRINGSIZE];
    struct ThreeDCoordinate tmp_3DC[3];


    memset( tmp_first, '\0', sizeof(tmp_first));
    memset( tmp_second, '\0', sizeof(tmp_second));
    memset( tmp_third, '\0', sizeof(tmp_third));
    memset( tmp_3DC, '\0', sizeof(tmp_3DC) );

    if( _VarType[2] == 'G' || (_VarType[2]=='C' && strstr(_MoveTo,"|")!=NULL) )
    {
        if( (tmp_head = strstr(_Info, _FileVarName)) == NULL || strstr(_Info,_FDS_ID) == NULL ) 
        {
            //printf( "failed to find varname=[%s] or FDS_ID=[%s] in [%s]!\n", _FileVarName, _FDS_ID, _Info );
            return -1;
        }

        sprintf( tmp_first, "%s", _Info); 

        tmp_head = strstr( tmp_first, _FileVarName );
        tmp_head = strstr( tmp_head, "=" );
        tmp_head ++;
        sprintf( tmp_second, "%s", tmp_head );

        //seperate the first part of _Info
        tmp_head[0] = '\0'; 

        tmp_tail = strstr( tmp_second, "/" ); 
        if( tmp_tail == NULL ) 
        {
            printf( "the line is not ended with a / \n" );
            return -1;
        }

        tmp_tail[0] = ',';
        tmp_tail = NULL;

        tmp_head = tmp_second;

        while(1)
        {
            tmp_tail = strstr(tmp_head, ","); 
            if( tmp_tail != NULL )
            {
                i++;
                tmp_head = tmp_tail;                
                tmp_head ++;
            }
            if( i== 6)
            {
                tmp_tail[0] = '\0'; //seperate the second part of _Info
                tmp_tail++;
                sprintf( tmp_third, "%s", tmp_tail ); //seperate the third part of _Info
                break;
            }
        }
        sscanf(tmp_second,"%lf,%lf,%lf,%lf,%lf,%lf", 
            &(tmp_3DC[0].x1), &(tmp_3DC[0].x2), &(tmp_3DC[0].y1), &(tmp_3DC[0].y2), &(tmp_3DC[0].z1), &(tmp_3DC[0].z2)); 
        sscanf(_BaseValue,"%lf|%lf|%lf|%lf|%lf|%lf", 
            &(tmp_3DC[1].x1), &(tmp_3DC[1].x2), &(tmp_3DC[1].y1), &(tmp_3DC[1].y2), &(tmp_3DC[1].z1), &(tmp_3DC[1].z2)); 
        
        if( fabs(tmp_3DC[0].x1 - tmp_3DC[1].x1) < ZERO && fabs(tmp_3DC[0].y1 - tmp_3DC[1].y1) < ZERO &&
            fabs(tmp_3DC[0].z1 - tmp_3DC[1].z1) < ZERO && fabs(tmp_3DC[0].x2 - tmp_3DC[1].x2) < ZERO &&
            fabs(tmp_3DC[0].y2 - tmp_3DC[1].y2) < ZERO && fabs(tmp_3DC[0].z2 - tmp_3DC[1].z2) < ZERO )
        {
           char tmp_MoveTo[MAXSTRINGSIZE];
           memset(tmp_MoveTo, '\0', sizeof(tmp_MoveTo));
           sprintf( tmp_MoveTo, "%s", _MoveTo );

           for( i=0; i< strlen(tmp_MoveTo); i++ )
           {
               if( tmp_MoveTo[i] == '|' )
                   tmp_MoveTo[i] = ',';
           } 

           // if _Pos is NULL, don't need to find the position of changed coordinator, namely VarType[2] = 'C' so that the file name will not include the changed coordinator
           if( _Pos != NULL ) { 
               if( FindPosDiff(tmp_MoveTo,tmp_second, (int *)_Pos ) == -1 )
               {
                   printf( "FindPosDiff() error: tmp_MoveTo = [%s], tmp_second=[%s] !\n", tmp_MoveTo, tmp_second );
                   return -1;
               }
           }

           trim(tmp_third,NULL);
           if( strlen(tmp_third) == 0 )
               strcat(tmp_MoveTo, "/");
           else
               strcat(tmp_MoveTo, ",");

           sprintf( _Info, "%s%s%s", tmp_first, tmp_MoveTo, tmp_third);
           return 0;
        }
    }


    if ( _VarType[2] == 'P' || (_VarType[2] == 'C' && strstr(_MoveTo,"|")==NULL))
    {
        if( (tmp_head = strstr(_Info, _FileVarName)) == NULL || strstr(_Info, _FDS_ID) == NULL ) 
        {
            printf( "failed to find varname=[%s] or FDS_ID=[%s] in [%s]!\n", _FileVarName, _FDS_ID, _Info );
            return -1;
        }

        sprintf( tmp_first, "%s", _Info); 

        tmp_head = strstr( tmp_first, _FileVarName );
        tmp_head = strstr( tmp_head, "=" );
        tmp_head ++;
        sprintf( tmp_second, "%s", tmp_head );
        tmp_head[0] = '\0';
        
        if( (tmp_tail = strstr( tmp_second, "/" ) ) == NULL )
        {
            printf( "the line is not ended with a / \n" );
            return -1;
        }
        tmp_tail == NULL;
        if( (tmp_tail = strstr( tmp_second, "," )) == NULL) // the value to be changed is not the last item
        {
            tmp_tail = strstr(tmp_second,"/"); // the value to be changed is the last item
        }
        sprintf( tmp_third, "%s", tmp_tail );
        sprintf( _Info, "%s%s%s", tmp_first, _MoveTo, tmp_third );    
        return 0;
    }    

    return -1;
}

/************************************************************************************************************************************************* 
 * Function: output files of non combined fire scenairo (with vartype of IS*, IR*)
 * _original_Info_Array: input parameter indicating a string array which has the content of a FDS baseline file
 * _gi: input parameter indicating the records needed to create new files based on baseline FDs file
 * Return: 0: success
 *         -1: Failure
 *************************************************************************************************************************************************/
int OutputSRFiles( char original_Info_Array[MAXLINENUM][MAXSTRINGSIZE], struct GenInfo *_gi )
{
    int i=0;
    char OutputFile[MAXSTRINGSIZE];

    for( i=0; i<MAXFILENUM; i++ )
    {
        int k=0;
        FILE *tmp_fp = NULL;
        //char *tmp_fn = "intermedium.fds";
        char tmp_fn[MAXSTRINGSIZE]; 
        char Info[MAXSTRINGSIZE];

/*
        if( _gi[i].VarType[0] != 'I' )
            continue;
*/ 
        if( strlen(_gi[i].VarType) == 0 )
            break;
        if( _gi[i].VarType[2] == 'C' )
            continue;

        memset( tmp_fn, 0x0, sizeof(tmp_fn));
        if( getcwd(tmp_fn, sizeof(tmp_fn) )== NULL )
        {
            printf( "getcwd error!\n");
            return -1;
        }

        strcat( tmp_fn, "/");
        strcat( tmp_fn, _gi[i].VarType);
        strcat( tmp_fn, "/temp.fds");
 
        tmp_fp = fopen( tmp_fn, "w+" );
        if( tmp_fp == NULL )
        {
            printf( "fopen %s error\n", tmp_fn );
            return -1;
        }

        memset(OutputFile, '\0', sizeof (OutputFile));

        //printf( " in OutputFile -  _gi[%d].VarType=%s, _gi.BaseValue=%s, _gi.MoveTo=%s\n", i, _gi[i].VarType, _gi[i].BaseValue, _gi[i].MoveTo);

        //printf( " in OutputFile - 2.%d2\n", i);

        while ( strlen(original_Info_Array[k]) > 0 )
        {
            char * tmp_ret = NULL;

            sprintf( Info, "%s", original_Info_Array[k] );

         //   printf( "before StrRpl: Info=%s, _gi[%d].VarType=%s, FDS_id=%s, FileVarName=%s,BaseValue=%s,MoveTo=%s\n", Info, i, _gi[i].VarType, _gi[i].FDS_ID, _gi[i].FileVarName, _gi[i].BaseValue, _gi[i].MoveTo );

            if( (tmp_ret=strstr(Info,_gi[i].FileVarName)) != NULL )
            {
               char * tmp_strp = NULL;
               int tmp_i=0;
               int tmp_Pos = -1; 
               
               if ( StrRpl( Info, _gi[i].VarType, _gi[i].FDS_ID,_gi[i].FileVarName, _gi[i].BaseValue, _gi[i].MoveTo,&tmp_Pos) == 0 )
               {
                    double tmp_6D[6];
                    char tmp_D_str[100];

          //     printf( "after StrRpl: Info=%s, _gi[%d].VarType=%s, FDS_id=%s, FileVarName=%s,BaseValue=%s,MoveTo=%s\n", Info, i, _gi[i].VarType, _gi[i].FDS_ID, _gi[i].FileVarName, _gi[i].BaseValue, _gi[i].MoveTo );
                    memset(tmp_6D, 0x0, sizeof(tmp_6D));
                    memset(tmp_D_str, 0x0, sizeof(tmp_D_str));

                    if( tmp_Pos != -1 ) // Geograpic variables
                        sprintf(OutputFile, "%c%d", _gi[i].VarType[2], tmp_Pos);     
                    else
                        sprintf(OutputFile, "%c", _gi[i].VarType[2]);     
    
                    strcat(OutputFile,"_" );
                    strcat(OutputFile,_gi[i].Alias);
                    strcat(OutputFile,"_" );

                    if( tmp_Pos != -1 )
                    {
                        get6Data(_gi[i].MoveTo,"|",tmp_6D);
                        sprintf(tmp_D_str,"%.3f", tmp_6D[tmp_Pos]);
                        strcat(OutputFile,tmp_D_str);
                    } else {
                        strcat(OutputFile, _gi[i].MoveTo);
                    }

                    strcat(OutputFile, ".fds");

                    for( tmp_i = 0; tmp_i < strlen(OutputFile); tmp_i++ )
                    {
                        if( OutputFile[tmp_i] == '|' )
                            OutputFile[tmp_i] = '_';
                    }
                    strcat(Info,"\r\n");
               }
            }
            //printf( "in first while 3- Info= %s, _gi[%d].FileVarName=%s\n", Info,i,_gi[i].FileVarName );

            //printf( "in first while 4- Info= %s, _gi[%d].FileVarName=%s\n", Info,i,_gi[i].FileVarName );
            fputs( Info, tmp_fp );
            memset(Info, '\0', sizeof(Info));
            k++;
         } 
         
         fclose(tmp_fp);

         printf( "before 0 OutputFile name is : [%s]\n", OutputFile );

         if( strlen(OutputFile) != 0 )
         {
             char tmp_ofn[MAXSTRINGSIZE];
             char *tmp_point = NULL;
             memset( tmp_ofn, 0x0, sizeof( tmp_ofn ) );


             if( getcwd( tmp_ofn, sizeof(tmp_ofn) ) == NULL )
             {
                 printf( "getcwd error !\n" );
                 return -1;
             }
             strcat( tmp_ofn, "/" );
             strcat( tmp_ofn, _gi[i].VarType);
             strcat( tmp_ofn, "/" );
             strcat( tmp_ofn, OutputFile);

             if( rename(tmp_fn, tmp_ofn) != 0 ) {
                 printf( "rename %s to %s failed!\n", tmp_fn, tmp_ofn );
                 return -1;
             }

             tmp_point = strstr( tmp_ofn, ".fds" );
             if( tmp_point != NULL ) // generate RPNUMSNR more FDS files to deal with the inconsistence of different FDS simulations with same file 
             {
                 int tmp_i = 0;
                 char tmp_RP_ofn[MAXSTRINGSIZE];
                 char tmp_SysCallStr[MAXSTRINGSIZE];

                 memset( tmp_SysCallStr, 0x0, sizeof( tmp_SysCallStr) );
                 sprintf( tmp_SysCallStr, "cp %s ", tmp_ofn );

                 *tmp_point = '\0'; // prepare for repeated file name;
                 for( tmp_i=0; tmp_i<RPNUMSNR; tmp_i++ )
                 {
                      memset( tmp_RP_ofn, 0x0, sizeof( tmp_RP_ofn ) );
                      sprintf( tmp_RP_ofn, "%s %s_%d.fds", tmp_SysCallStr, tmp_ofn, tmp_i );
                      printf( "system call : [%s]\n", tmp_RP_ofn );
                      system(tmp_RP_ofn);
                 }
                 
             } else {
                 printf( "no .fds in [%s]\n", tmp_ofn );
             }
             
             
         } else {
            
            printf( "cannot find the informaiton in baseline fds file: gi[%d], VarType=%s, FileVarName=%s, BaseValue=%s, MoveTo=%s\n", 
                                     i,_gi[i].VarType, _gi[i].FileVarName, _gi[i].BaseValue, _gi[i].MoveTo );
            return -1;
         }
        
         printf( "OutputFile name is : [%s]\n", OutputFile );
    }
    return 0;
}

/************************************************************************************************************************************************* 
 * Function: output files of combined fire scenairo (with vartype of I*C )
 * InfoArray: input parameter indicating a string array which has the content of a FDS baseline file. this array will be changed and written to a new file
 * medium_InfoArray: input parameter indicating a string array which has the content of a FDS baseline file. this array works as an intermediate array
 * _original_Info_Array: input parameter indicating a string array which has the content of a FDS baseline file.this array will not be modified
 * _gi: input parameter indicating the records needed to create new files based on baseline FDs file
 * Return: 0: success
 *         -1: Failure
 *************************************************************************************************************************************************/
int OutputCmbFiles(struct GenInfo *_gi, char InfoArray[MAXLINENUM][MAXSTRINGSIZE], char medium_InfoArray[MAXLINENUM][MAXSTRINGSIZE], char original_InfoArray[MAXLINENUM][MAXSTRINGSIZE])
{
    int i=0;
    int OC_flag = 0;
    char OutputFileCmb[MAXSTRINGSIZE];
    char TmpOutputFileCmb[MAXSTRINGSIZE];
    char OC_VarType[4];
    
    memset(OutputFileCmb, '\0', sizeof (OutputFileCmb));
    memset(TmpOutputFileCmb, '\0', sizeof (TmpOutputFileCmb));

    //printf( "sizeof InfoArray = %ld", MAXLINENUM*MAXSTRINGSIZE );

    for( i=0; i<MAXFILENUM; i++ )
    {
        FILE *tmp_fp = NULL;
        char tmp_fn[MAXSTRINGSIZE]; 

/*
        if( _gi[i].VarType[0] != 'I' )
            continue;
*/

        if( _gi[i].VarType[2] == 'C' ) // dealing with VarType = "I*C"
        {
            int k=0;

            //the whole output file name will include the vartype. since one vartype may cover many files, only one is needed to be in the whole output file name
            if( strstr( OutputFileCmb, _gi[i].VarType) == NULL) { 

                if (getcwd(TmpOutputFileCmb, sizeof(TmpOutputFileCmb)) != NULL) {
                    printf("Current working dir: %s\n",TmpOutputFileCmb);
                } else {
                    perror("getcwd() error");
                    return -1;
                }         
                strcat( TmpOutputFileCmb, "/" );
                strcat( TmpOutputFileCmb, _gi[i].VarType );
                strcat( TmpOutputFileCmb, "/" );
            }

            while( k < MAXLINENUM )
            {
                if( strstr(medium_InfoArray[k], _gi[i].FileVarName) != NULL )
                { 

                    if( StrRpl( medium_InfoArray[k], _gi[i].VarType,_gi[i].FDS_ID, _gi[i].FileVarName, _gi[i].BaseValue, _gi[i].MoveTo, NULL) == 0 )
                    {
                    printf( "after StrRpl: gi[%d], Info=%s, VarType=%s, FileVarName=%s, BaseValue=%s, MoveTo=%s\n", 
                         i, medium_InfoArray[k], _gi[i].VarType, _gi[i].FileVarName, _gi[i].BaseValue, _gi[i].MoveTo );
                        strcat(medium_InfoArray[k],"\r\n");
                        //flag = 1;
                        OC_flag ++;
                        if( OC_flag == 1 )
                        {
                            sprintf(OC_VarType, "%s", _gi[i].VarType ); // save the VarType of one combination file
                            sprintf( InfoArray[k], "%s", medium_InfoArray[k] );
                        } else {
                          if( strcmp(OC_VarType, _gi[i].VarType) == 0 ) // they are in the same VarType, so in the same file
                          { 
                             sprintf( InfoArray[k], "%s", medium_InfoArray[k] );
                          } else {
                             OC_flag = 9999;
                             break;
                          }
                          
                        }                        
                    }

                }
                k++;
            }

            strcat(TmpOutputFileCmb, _gi[i].Alias);
            strcat(TmpOutputFileCmb, "_");

            printf( "OC_flag = [%d], OC_VarType = %s, i=%d, _gi.VarType=%s,OutputFileCmb=[%s], TmpOutputFileCmb=%s\n", OC_flag,OC_VarType, i, _gi[i].VarType,OutputFileCmb, TmpOutputFileCmb );

            if( (i<MAXFILENUM-1) && (strlen(_gi[i+1].VarType)==0 )) // the last entry in _gi
            {
                OC_flag = 9999;
                memset(OutputFileCmb,0x0,sizeof(OutputFileCmb));
                sprintf(OutputFileCmb, "%s", TmpOutputFileCmb); 
                i++; // avoid endless running since we have a i-- in the next if sentence 
            } 

            if( OC_flag == 9999 ) //indicates that one combined FDS file should be generated
            {
                char tmp_str[MAXSTRINGSIZE];
                int tmp_k=0;
                FILE *I_C_fp= NULL;

                memset(tmp_str, 0x0, sizeof(tmp_str));
                sprintf( tmp_str, "%s", OutputFileCmb);
                
                if( OutputFileCmb[strlen(OutputFileCmb)-1] == '_' )
                {
                    printf( "OutputFileCmb [%s], strlen = %d, the last char is : %c \n", OutputFileCmb, strlen(OutputFileCmb),  OutputFileCmb[strlen(OutputFileCmb)-1] );
                    OutputFileCmb[strlen(OutputFileCmb)-1] = '.';
                }

                strcat(OutputFileCmb, "fds" );
                printf( " after strcat : OutputFileCmb [%s], strlen = %d, the last char is : %c \n", OutputFileCmb, strlen(OutputFileCmb),  OutputFileCmb[strlen(OutputFileCmb)-1] );

                I_C_fp = fopen(  OutputFileCmb, "w+" );
                if( I_C_fp != NULL )
                {
                    while( strlen(InfoArray[tmp_k]) > 0 && tmp_k< MAXLINENUM )
                    {
                        //printf( "InfoArray[%d] = %s\n", tmp_k, InfoArray[tmp_k] );
                        fputs(InfoArray[tmp_k],I_C_fp);
                        tmp_k++;
                    }
                }else {
                        printf( "open fire [%s] error!", OutputFileCmb );
                        return -1;
                }

                fclose( I_C_fp );

                // generate RPNUMCMB more files to deal with the inconsistence of FDS simulations with identical FDS files
                for( tmp_k=0; tmp_k<RPNUMCMB; tmp_k++ )
                {
                         char tmp_SysCallStr[MAXSTRINGSIZE];
                         memset( tmp_SysCallStr, 0x0, sizeof( tmp_SysCallStr));
                         sprintf( tmp_str, "%s_%d.fds", tmp_str, tmp_k );
                         sprintf( tmp_SysCallStr, "cp %s %s", OutputFileCmb, tmp_str ); 
                         memset( tmp_str, 0x0, sizeof(tmp_str));
                         system(tmp_SysCallStr);
                }

                memset( InfoArray, 0x0, sizeof(InfoArray));
                memcpy( InfoArray, original_InfoArray, sizeof(InfoArray));

                i--; // i needs to go back one step becasue currently it points to a new I*C type, i++ in for sentence will be conducted later
                OC_flag = 0;
                memset(OutputFileCmb,0x0,sizeof(OutputFileCmb));
            } else {
                memset(OutputFileCmb,0x0,sizeof(OutputFileCmb));
                sprintf(OutputFileCmb, "%s", TmpOutputFileCmb); 
            }
            // here medium_InfoArray is only a pointer,its size if 8, not what we expect 
            //memset( medium_InfoArray, 0x0, sizeof(medium_InfoArray));
            //memcpy( medium_InfoArray, original_InfoArray, sizeof(original_InfoArray));
            memset( medium_InfoArray, 0x0, MAXLINENUM*MAXSTRINGSIZE);
            memcpy( medium_InfoArray, original_InfoArray, MAXLINENUM*MAXSTRINGSIZE);
        }
    }
    return 0;
}

/************************************************************************************************************************************************* 
 * Function: 1. if one FDS file record takes up more than one lines, then align them into one line, 2. copy the aligned FDS lines to the three string arraies
 * _fp: input parameter indicating a FDS file pointer
 * _InfoArray: output parameter holding the FDS file content
 * _medium_InfoArray: output parameter holding the FDS file content
 * _original_InfoArray: output parameter holding the FDS file content
 * Return: 0: success
 *         -1: failure
 *************************************************************************************************************************************************/
int AlignFDS(FILE *_fp, char _InfoArray[MAXLINENUM][MAXSTRINGSIZE], char _medium_InfoArray[MAXLINENUM][MAXSTRINGSIZE], char _original_InfoArray[MAXLINENUM][MAXSTRINGSIZE])
{
    int i=0;
    char Info[MAXSTRINGSIZE];

    memset( Info, 0x0, sizeof(Info) );

    while ( fgets( Info, sizeof(Info), _fp) != NULL )
    {
        if( strstr( Info, "&" ) != NULL )
        {
            char * tmp_ret = NULL;
            tmp_ret = strstr(Info, "/" );
            if( tmp_ret == NULL )
            {
              char tmp_str[MAXSTRINGSIZE];
              memset(tmp_str, '\0', sizeof(tmp_str));
              while ( fgets( tmp_str, sizeof(tmp_str), _fp)!= NULL )
              {
                  strcat( Info, tmp_str);
                  trim(tmp_str, NULL );
                  if( tmp_str[strlen(tmp_str)-1] == '/' )
                  {
                      break;
                  }
              }
            }
        }

        sprintf(_medium_InfoArray[i], "%s", Info );
        sprintf(_original_InfoArray[i], "%s", Info );
        sprintf(_InfoArray[i], "%s", Info );

        if( i++  == MAXLINENUM )
        {
            printf( "file %s is too big!\n", _fp );
            return -1;
        }
    }
}


