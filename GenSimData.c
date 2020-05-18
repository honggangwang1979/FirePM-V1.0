#include  "FirePM.h"

//this is a tool  to simulate the generation process of the dynamically changed input file (Dyn.txt)
int main()
{
    int record=0;
    char *HL = "Sequence,Var1,Var2,Var3,Var4\nTime,CDW,SY,EX,HRR\n";
    //char *HL = "Sequence,Var1,Var2,Var3\nTime,PT,NIP,VL\n";
    char tmp_line[MAXSTRINGSIZE];
    FILE *fp=NULL;

    
    memset(tmp_line, 0x0, sizeof(tmp_line) );
    
    while(1)
    {
        char str[MAXSTRINGSIZE];
        double x1=0.0,SY=0.0,EX=0.0, HRR=0.0;
        //double PT=0.0,NIP=0.0,VL=0.0;

        memset( str, 0x0, sizeof(str) );

        sleep(1);

        fp= fopen("Dyn.txt", "w+" );
        if( fp==NULL)
        {
            printf( "open Dyn.txt error !\n" );
            return -1;
        }

        fprintf( fp, "%s", HL );       
        x1 = rand_double2( -44.8, -43.6 );
        SY = rand_double2( 0.01, 0.09 );
        EX = rand_double2( 0.05, 0.5 );
        HRR= rand_double2( 1200, 4800 );
        sprintf(str, "%d,%.3lf|-43.5|-39.7|-39.475|-0.025|2.5,%.4f,%.2f,%.2f\n", record++, x1, SY, EX, HRR );	 
/*
        PT = rand_double2( 250, 350 );
        NIP = rand_double2( 150, 250 );
        VL = rand_double2( 0.7, 1.3);
        sprintf(str, "%d,%.2f,%.2f,%.2f\n", record++, PT, NIP, VL);	 
*/
        fprintf(fp,str);

        printf("\t\t\t%s\r",trim(str,NULL));
        fflush(stdout);
        fclose(fp);
    }

    return 0;
}
