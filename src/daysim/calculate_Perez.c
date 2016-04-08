/* This file contains large pieces of the src file for the 		*/
/* RADIANCE program gendaylit, which has been written by J.J. Delaunay 	*/
/* at the Fraunhofer Institute for Solar Energy Systems  		*/
/* changes have been introduced by written by Christoph Reinhart to 	*/
/* implement the Perez sky model into the DAYSIM simulation environment.*/
/* last changed in February 2001					*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <errno.h>

#include "fropen.h"
#include "read_in_header.h"
#include "sun.h"

#include "ds_illum.h"


int write_segments_diffuse(double dir,double dif);
int write_segments_direct(double dir,double dif, int *number_direct_coefficients,
						  int *shadow_testing_on, int count_var, int current_shadow_testing_value);
int shadow_test_single(double sun_x, double sun_y, double sun_z);


#define  WHTEFFICACY            179.

/* constants */
const double	AU = 149597890E3;
const double 	solar_constant_e = 1367;    /* solar constant W/m^2 */
const double  	solar_constant_l = 127.5;   /* solar constant klux */
const double	half_sun_angle = 0.2665;
const double	skyclearinf = 1.000;	/* limitations for the variation of the Perez parameters */
const double	skyclearsup = 12.1;
const double	skybriginf = 0.01;
const double	skybrigsup = 0.6;
/* Perez sky parametrization : epsilon and delta calculations from the direct and diffuse irradiances */
double sky_brightness();
double sky_clearness();
/* calculation of the direct and diffuse components from the Perez parametrization */
double	diffus_irradiance_from_sky_brightness();
double 	direct_irradiance_from_sky_clearness();
/* misc */
double 	c_perez[5];
void calculate_perez();
float 	lv_mod[145];  /* 145 luminance values*/
float 	*theta_o, *phi_o, *coeff_perez;
/* Perez global horizontal, diffuse horizontal and direct normal luminous efficacy models : input w(cm)=2cm, solar zenith angle(degrees); output efficacy(lm/W) */
double 	glob_h_effi_PEREZ();
double 	glob_h_diffuse_effi_PEREZ();
double 	direct_n_effi_PEREZ();
/*likelihood check of the epsilon, delta, direct and diffuse components*/
void 	check_parametrization();
void 	check_irradiances();
void 	check_illuminances();
void 	illu_to_irra_index();
/* Perez sky luminance model */
int 	lect_coeff_perez(float **coeff_perez);
double  calc_rel_lum_perez(double dzeta,double gamma,double Z,
						   double epsilon,double Delta,float *coeff_perez);
/* coefficients for the sky luminance perez model */
void 	coeff_lum_perez(double Z, double epsilon, double Delta, float *coeff_perez);
double	radians(double degres);
double	degres(double radians);
void	theta_phi_to_dzeta_gamma(double theta,double phi,double *dzeta,double *gamma, double Z);
double 	integ_lv(float *lv,float *theta);
float 	*theta_ordered();
float 	*phi_ordered();
/* astronomy and geometry*/
double 	get_eccentricity();
double 	air_mass();
double 	get_angle_sun_direction(double sun_zenith, double sun_azimut, double direction_zenith, double direction_azimut);
/* definition of the sky conditions through the Perez parametrization */
double	skyclearness, skybrightness;

//radiance of the sun disk and of the circumsolar area
double	solarradiance_visible_radiation;
double	solarradiance_solar_radiation;
double	solarradiance_luminance;

double	diffusilluminance, directilluminance, diffusirradiance, directirradiance;
double	sunzenith, daynumber=150, atm_preci_water=2;
double 	diffnormalization, dirnormalization;
double  diffnormalization_visible_radiation;
double  diffnormalization_solar_radiation;
double	diffnormalization_luminance;

/* default values */
int  cloudy = 0;				/* 1=standard, 2=uniform */
int  dosun = 1;
double  zenithbr_visible_radiation = -1.0;
double  zenithbr_solar_radiation = -1.0;
double  zenithbr_luminance = -1.0;

double  betaturbidity = 0.1;
/* computed values */
double  sundir[3];
double  groundbr_visible_radiation;
double 	groundbr_solar_radiation;
double 	groundbr_luminance;
double  F2;

/* defines current time */
float hour;
int day, month;


char temp_file[200];/* ="/tmp/temporary.oct";*/


void add_time_step(float time_step)
{
	div_t e;
	hour+=(time_step/60.0);
	e=div(hour,24);		/* hours*/
	day+=e.quot;		/* days */
	hour =hour-(e.quot)*(24.0);

	if(month==1 && day>31){month++;day-=31;}
	if(month==2 && day>28){month++;day-=28;}
	if(month==3 && day>31){month++;day-=31;}
	if(month==4 && day>30){month++;day-=30;}
	if(month==5 && day>31){month++;day-=31;}
	if(month==6 && day>30){month++;day-=30;}
	if(month==7 && day>31){month++;day-=31;}
	if(month==8 && day>31){month++;day-=31;}
	if(month==9 && day>30){month++;day-=30;}
	if(month==10 && day>31){month++;day-=31;}
	if(month==11 && day>30){month++;day-=30;}
	if(month==12 && day>31){month=1;day-=31;}
}

void calculate_perez(int *shadow_testing_on, int *dir_rad,int *number_direct_coefficients)
{
    

	int  reset=0;
	float dir1=0.0,dif1=0.0,hour_bak=0.0;
	double dir=0.0 ,dif=0.0;
	float header_latitude;
	float header_longitude;
	float header_time_zone;
	float header_site_elevation;
	int header_implemented_in_wea=0;
	int shadow_testing_counter=0;
	int current_shadow_testing_value=0;
	int header_weather_data_short_file_units;
	float sunrise,sunset;
	int i=0,j=0,k=0,m=0,number_data_values=0;
	char keyword[200]="";
    month=start_month;
	day=start_day;
	hour=start_hour;

	/* normalization factor for the relative sky luminance distribution, diffuse part*/
	if ( (coeff_perez = malloc(8*20*sizeof(float))) == NULL )
		{fprintf(stdout,"ds_illum: Out of memory error in function main !");exit(1);}


	/* normalization factor for the relative sky luminance distribution, diffuse part*/


	/* read the coefficients for the Perez sky luminance model */
	lect_coeff_perez( &coeff_perez) ;

	/* read the angles */
	theta_o = theta_ordered();
	phi_o = phi_ordered();

	if( *shadow_testing_on){
		shadow_testing(dir_rad,number_direct_coefficients);
		fprintf(stdout,"ds_illum: initial shadow_testing done \n");
		*shadow_testing_on=shadow_testing_new;
	}


	/* FILE I/O */
	INPUT_DATAFILE=open_input(input_datafile);
	for (k=0 ; k<TotalNumberOfDCFiles ; k++) {
		if( strcmp( shading_illuminance_file[k+1], "-" ) == 0 )
			SHADING_ILLUMINANCE_FILE[k]= stdout;
		else
		{
			//printf("shading file %d %s\n",k,shading_illuminance_file[k+1]);
			SHADING_ILLUMINANCE_FILE[k]=open_output(shading_illuminance_file[k+1]);
		}
	}

	/* get number of data values and test whether the climate input file has a header */
	fscanf(INPUT_DATAFILE,"%s", keyword);
	if( !strcmp(keyword,"place") ){
		header_implemented_in_wea=1;
		fscanf(INPUT_DATAFILE,"%*[^\n]");fscanf(INPUT_DATAFILE,"%*[\n\r]");
		//latitude
		fscanf(INPUT_DATAFILE,"%s %f", keyword, &header_latitude);
		header_latitude*= (M_PI/180.);
		if((header_latitude-s_latitude)>5*(M_PI/180.)||(header_latitude-s_latitude)<-5*(M_PI/180.)){
			fprintf(stderr,"ds_illum WARNING: latitude in climate file header (wea) and latitude on project file (hea) differ by more than 5 DEG.\n");
		}
		//longitude
		fscanf(INPUT_DATAFILE,"%s %f", keyword, &header_longitude);
		header_longitude*= (M_PI/180.);
		if((header_longitude-s_longitude)>5*(M_PI/180.)||(header_longitude-s_longitude)<-5*(M_PI/180.)){
			fprintf(stderr,"ds_illum WARNING: longitude in climate file header (wea) and longitude on project file (hea) differ by more than 5 DEG.\n");
		}
		//time_zone
		fscanf(INPUT_DATAFILE,"%s %f", keyword, &header_time_zone);
		header_time_zone*= (M_PI/180.);
		if((header_time_zone-s_meridian)>5*(M_PI/180.)||(header_time_zone-s_meridian)<-5*(M_PI/180.)){
			fprintf(stderr,"ds_illum WARNING: time zone in climate file header (wea) and time zone on project file (hea) differ by more than 5 DEG.\n");
		}
		//site_elevation
		fscanf(INPUT_DATAFILE,"%s %f", keyword, &header_site_elevation);
		//weather_data_file_units
		fscanf(INPUT_DATAFILE,"%s %d", keyword, &header_weather_data_short_file_units);
		if(header_weather_data_short_file_units !=wea_data_short_file_units){
			fprintf(stderr,"ds_illum WARNING: climate file units differ in climate and project header files.\n");
		}
		number_data_values--;
	}


	while( EOF != fscanf(INPUT_DATAFILE,"%*[^\n]")){number_data_values++;fscanf(INPUT_DATAFILE,"%*[\n\r]");}
	close_file(INPUT_DATAFILE);

	INPUT_DATAFILE=open_input(input_datafile);
	if(header_implemented_in_wea){
		/* skip header */
		fscanf(INPUT_DATAFILE,"%*[^\n]");fscanf(INPUT_DATAFILE,"%*[\n\r]");
		fscanf(INPUT_DATAFILE,"%*[^\n]");fscanf(INPUT_DATAFILE,"%*[\n\r]");
		fscanf(INPUT_DATAFILE,"%*[^\n]");fscanf(INPUT_DATAFILE,"%*[\n\r]");
		fscanf(INPUT_DATAFILE,"%*[^\n]");fscanf(INPUT_DATAFILE,"%*[\n\r]");
		fscanf(INPUT_DATAFILE,"%*[^\n]");fscanf(INPUT_DATAFILE,"%*[\n\r]");
		fscanf(INPUT_DATAFILE,"%*[^\n]");fscanf(INPUT_DATAFILE,"%*[\n\r]");
	}

	for (m=0 ; m<number_data_values ; m++){

			fscanf(INPUT_DATAFILE,"%d %d %f %f %f",&month,&day,&hour,&dir1,&dif1);
			dir=dir1;
			dif=dif1;
			centrum_hour=hour;
			sunrise=12+12-stadj(jdate(month, day))-solar_sunset(month,day);
			sunset=solar_sunset(month,day)-stadj(jdate(month, day));
			if( ( hour-(0.5*time_step/60.0)<=sunrise ) && ( hour +(0.5*time_step/60.0)> sunrise )){
				hour=0.5*(hour +(0.5*time_step/60.0) )+0.5*sunrise;
			}else{
				if( ( hour-(0.5*time_step/60.0)<=sunset ) && ( hour +(0.5*time_step/60.0)> sunset )){
					hour=0.5*(hour -(0.5*time_step/60.0) )+0.5*sunset;
				}
			}

		for (k=0 ; k<TotalNumberOfDCFiles ; k++)
			fprintf(SHADING_ILLUMINANCE_FILE[k],"%d %d %.3f ",month,day,centrum_hour );

		//assign value of current shadow test: "Is the sensor in the sun?"
		if( *shadow_testing_on &&dir >= dir_threshold && dif >= dif_threshold)
			current_shadow_testing_value=shadow_testing_results[shadow_testing_counter++];


		if( (dif<dif_threshold)
			|| ( salt( sdec(jdate(month, day)),hour+stadj(jdate(month, day))) <0) ) {
			for (k=0 ; k < TotalNumberOfDCFiles; k++){
				for (j=0 ; j<number_of_sensors ; j++)
					fprintf(SHADING_ILLUMINANCE_FILE[k]," %.0f",0.0);
			}
			if ((dif>dif_threshold)
				&&( salt( sdec(jdate(month, day)),hour+stadj(jdate(month, day))) <0 )
				&&all_warnings ) {
				fprintf(stdout,"ds_illum: warning - sun below horizon at %d %d %.3f (solar altitude: %.3f)\n",month,day,hour, 57.38*salt( sdec(jdate(month, day)),hour+stadj(jdate(month, day))) );}
		}else{
			write_segments_diffuse(dir,dif);
			write_segments_direct(dir,dif,number_direct_coefficients, shadow_testing_on,0,current_shadow_testing_value);
		}

		/* End of Line */
		for (k=0 ; k<TotalNumberOfDCFiles ; k++)
			fprintf(SHADING_ILLUMINANCE_FILE[k],"\n");

		if(reset){hour=hour_bak;reset=0;}
	}
	i=close_file(INPUT_DATAFILE);
		for (k=0 ; k<TotalNumberOfDCFiles ; k++)
			close_file(SHADING_ILLUMINANCE_FILE[k]);
}


int get_sky_patch_number( float Dx,float  Dy,float Dz)
{
	int i=0,j=0,k=0,l=0;
	double a=0;
	int  patches[8];
	patches[0]=30;
	patches[1]=30;
	patches[2]=24;
	patches[3]=24;
	patches[4]=18;
	patches[5]=12;
	patches[6]=6;
	patches[7]=1;

	a=(180/M_PI)*asin(Dz);
	if( (a>=0.0)&&(a<12.0)){j=0;k=0;}
	if( (a>=12.0)&&(a<24.0)){j=1;k=30;}
	if( (a>=24.0)&&(a<36.0)){j=2;k=60;}
	if( (a>=36.0)&&(a<48.0)){j=3;k=84;}
	if( (a>=48.0)&&(a<60.0)){j=4;k=108;}
	if( (a>=60.0)&&(a<72.0)){j=5;k=126;}
	if( (a>=72.0)&&(a<84.0)){j=6;k=138;}
	if( (a>=84.0)&&(a<90.0)){j=7;k=144;i=144;}
	/*printf(" %f %f\n",a,Dz);*/
	a=(180.0/M_PI)*atan2(Dy,Dx);
	for (l=0 ; l<patches[j]; l++){
		if(((l+1)*(360.0/patches[j])>=a )&& ((l)*(360.0/patches[j])<a )){i=k+l;l=patches[j];}
	}

	return(i);
}


double skybright(double Dx,double Dy,double Dz,float A1,float A2, float A3,float A4,
				 float A5,float A6,float A7,float A8,float A9, float A10,double dir,
				 int number)
{
	/* Perez Model */
	double illuminance, intersky,  gamma;

	if(( Dx*A8 + Dy*A9 + Dz*A10)<1 ) {
		gamma = acos(Dx*A8 + Dy*A9 + Dz*A10);
	} else {
		gamma=0.0093026;
	}
	if ( (gamma < 0.0093026) ){ gamma=0.0093026;}

	if(Dz >0.01) {
		intersky= A1* (1 + A3*exp(A4/Dz  ) ) * ( 1 + A5*exp(A6*gamma) + A7*cos(gamma)*cos(gamma) );
	} else {
		intersky= A1* (1 + A3*exp(A4/0.01) ) * ( 1 + A5*exp(A6*gamma) + A7*cos(gamma)*cos(gamma) ) ;
	}

	illuminance=(pow(Dz+1.01,10)*intersky+pow(Dz+1.01,-10)*A2)/(pow(Dz+1.01,10)+pow(Dz+1.01,-10));

	if (  illuminance <0 ) illuminance =0;
	return(illuminance);
}

double CIE_SKY(double Dz,float A2,float A3)
{	/* CIE overcast sky */
	double illuminance, intersky;

	intersky= A2 * (1 + 2*Dz)/3;

	illuminance=(pow(Dz+1.01,10)*intersky+pow(Dz+1.01,-10)*A3)/(pow(Dz+1.01,10)+pow(Dz+1.01,-10));
	return(illuminance);
}

// =========================================================
// This function calculates the Sky lumiance distribution
// and lumiance efficacy model based on direct and diffuse
// irradiances/illuminances (depending on input climate file
// =========================================================
int write_segments_diffuse(double dir,double dif)
{
    
	int j, jd;
	double 	dzeta, gamma;
	double	normfactor=0.0;
	double altitude,azimuth;
	int S_INTER=0;
	double  A1,A2,A3,A4,A5,A6,A7;
	double reduction=1.0;
	double  sd, st;

	jd = jdate(month, day);		/* Julian date */
	sd = sdec(jd);			/* solar declination */
	st = hour + stadj(jd);
	altitude = salt(sd, st);
	azimuth = sazi(sd, st);
	daynumber = (double)jdate(month, day);
	sundir[0] = -sin(azimuth)*cos(altitude);
	sundir[1] = -cos(azimuth)*cos(altitude);
	sundir[2] = sin(altitude);
	sunzenith = 90 - altitude*180/M_PI;

	/* compute the inputs for the calculation of the light distribution over the sky*/
	/*input = 1; input_unities 1	 dir normal Irr [W/m^2] dif hor Irr [W/m^2] */
	if (input_unities==1){	directirradiance = dir;
		diffusirradiance = dif;}
	/*input = 2; input_unities 3 dir hor Irr [W/m^2] dif hor Irr [W/m^2] */
	if (input_unities==2){	directirradiance = dir;
		diffusirradiance = dif;}

	/*input = 3; input_unities 2 dir nor Ill [Lux] dif hor Ill [Lux] */
	if (input_unities==3){	directilluminance = dir;
		diffusilluminance = dif;}

	//output 	OutputUnits
	//  	0	W visible
	//  	1	W solar radiation
	//  	2	lumen

	// compute the inputs for the calculation of the light distribution over the sky

	if (input_unities==1 || input_unities==2) {
		if (input_unities==2){
			if (altitude*180/M_PI<1.0)
				{directirradiance=0;diffusirradiance=0;}
			else {
				if (directirradiance>0.0)
					directirradiance=directirradiance/sin(altitude);
			}
		}
		check_irradiances();
		skybrightness = sky_brightness();
		skyclearness =  sky_clearness();
		check_parametrization();
		
		diffusilluminance = diffusirradiance*glob_h_diffuse_effi_PEREZ();/*diffuse horizontal illuminance*/
		directilluminance = directirradiance*direct_n_effi_PEREZ();
		check_illuminances();
		
	}
	if (input_unities==3){
		check_illuminances();
		illu_to_irra_index();
		check_parametrization();
	}


	/* parameters for the perez model */
	coeff_lum_perez(radians(sunzenith), skyclearness, skybrightness, coeff_perez);

	/*calculation of the modelled luminance */
	for (j=0;j<145;j++)	{
		theta_phi_to_dzeta_gamma(radians(*(theta_o+j)),radians(*(phi_o+j)),&dzeta,&gamma,radians(sunzenith));
		lv_mod[j] = calc_rel_lum_perez(dzeta,gamma,radians(sunzenith),skyclearness,skybrightness,coeff_perez);
	}

	/* integration of luminance for the normalization factor, diffuse part of the sky*/
	diffnormalization = integ_lv(lv_mod, theta_o);


	/*normalization coefficient in lumen or in watt*/
	//OutputUnits==0 visible W
	diffnormalization_visible_radiation = diffusilluminance/diffnormalization/WHTEFFICACY;

	//OutputUnits==1 solar radiation
	diffnormalization_solar_radiation = diffusirradiance/diffnormalization;

	// OutputUnits==3 illuminance/luminance
	diffnormalization_luminance = diffusilluminance/diffnormalization;


	/* calculation for the solar source */
	solarradiance_visible_radiation = directilluminance/(2*M_PI*(1-cos(half_sun_angle*M_PI/180)))/WHTEFFICACY;
	solarradiance_solar_radiation = directirradiance/(2*M_PI*(1-cos(half_sun_angle*M_PI/180)));
	solarradiance_luminance = directilluminance/(2*M_PI*(1-cos(half_sun_angle*M_PI/180)));


	/* Compute the ground radiance */
	zenithbr_visible_radiation=calc_rel_lum_perez(0.0,radians(sunzenith),radians(sunzenith),skyclearness,skybrightness,coeff_perez);
	zenithbr_solar_radiation=zenithbr_visible_radiation;
	zenithbr_luminance=zenithbr_visible_radiation;

	zenithbr_visible_radiation*=diffnormalization_visible_radiation;
	zenithbr_solar_radiation*=diffnormalization_solar_radiation;
	zenithbr_luminance*=diffnormalization_luminance;
	if (skyclearness==1)
		normfactor = 0.777778;

	if (skyclearness>=6)
		{
			F2 = 0.274*(0.91 + 10.0*exp(-3.0*(M_PI/2.0-altitude)) + 0.45*sundir[2]*sundir[2]);
			normfactor = normsc(altitude,S_INTER)/F2/M_PI;
		}

	if ( (skyclearness>1) && (skyclearness<6) )
		{
			S_INTER=1;
			F2 = (2.739 + .9891*sin(.3119+2.6*altitude)) * exp(-(M_PI/2.0-altitude)*(.4441+1.48*altitude));
			normfactor = normsc(altitude,S_INTER)/F2/M_PI;
		}

	groundbr_visible_radiation = zenithbr_visible_radiation*normfactor;
	groundbr_solar_radiation = zenithbr_solar_radiation*normfactor;
	groundbr_luminance = zenithbr_luminance*normfactor;

	if (dosun&&(skyclearness>1))
		{
			groundbr_visible_radiation += 6.8e-5/M_PI*solarradiance_visible_radiation*sundir[2];
			groundbr_solar_radiation += 6.8e-5/M_PI*solarradiance_solar_radiation*sundir[2];
			groundbr_luminance += 6.8e-5/M_PI*solarradiance_luminance*sundir[2];
		}
	groundbr_visible_radiation *= gprefl;
	groundbr_solar_radiation *= gprefl;
	groundbr_luminance *= gprefl;


	/*=============================*/
	/*=== diffuse contribution ====*/
	/*=============================*/


	if (dosun&&(skyclearness==1))
		{
			solarradiance_visible_radiation=0;
			solarradiance_solar_radiation=0;
			solarradiance_luminance=0;
		}


	/*change input irradiances according to the quantities specified in the header*/
	reduction=1.0;
	if(solar_pen){
		reduction=0.75;
		if(altitude> lower_lat && altitude < upper_lat){
			if(azimuth> lower_azi && azimuth<upper_azi){
				if(dir>=50){
					reduction=reduction_diffuse;
					solarradiance_visible_radiation=0;
					solarradiance_solar_radiation=0;
					solarradiance_luminance=0;
				}
			}
		}
	}

			//assign Perez Coefficients
			A1=diffnormalization_luminance;	/*A1			- diffus normalization*/
			A2=groundbr_luminance;			/*A2			- ground brightness*/
			A3= c_perez[0];				/*A3            - coefficients for the Perez model*/
			A4= c_perez[1];
			A5= c_perez[2];
			A6= c_perez[3];
			A7= c_perez[4];


			for (j=0 ; j<145 ; j++){
				SkyPatchLuminance[j]=horizon_factor[j]*skybright(Dx_dif_patch[j],Dy_dif_patch[j],Dz_dif_patch[j],A1,A2,A3,A4,A5,A6,A7,sundir[0],sundir[1],sundir[2],dir,j);
				if( horizon_factor[j]<1)
					SkyPatchLuminance[j]+=(1-horizon_factor[j])*skybright(0.9961946,0,-0.087156,A1,A2,A3,A4,A5,A6,A7,sundir[0],sundir[1],sundir[2],dir,0);
				if(reduction<1.0)
					SkyPatchLuminance[j]*=reduction;
			}

			// assign ground brightness
			if(dds_file_format) // new dds file format chosen
				{
					/* 0 to 10 */  SkyPatchLuminance[145]=skybright(0.9397,0,-0.342,A1,A2,A3,A4,A5,A6,A7,sundir[0],sundir[1],sundir[2],dir,0);
					//		/* 0 to 10 */  SkyPatchLuminance[145]=skybright(0.9961946,0,-0.087156,A1,A2,A3,A4,A5,A6,A7,sundir[0],sundir[1],sundir[2],dir,0);
				}else{
				/* 0 to 10 */  SkyPatchLuminance[145]=skybright(0.9961946,0,-0.087156,A1,A2,A3,A4,A5,A6,A7,sundir[0],sundir[1],sundir[2],dir,0);
				/* 10 to 30*/  SkyPatchLuminance[146]=skybright(0.9397,0,-0.342,A1,A2,A3,A4,A5,A6,A7,sundir[0],sundir[1],sundir[2],dir,0);
				/* 30 to 90 */ SkyPatchLuminance[147]=skybright(0,0,-1.0,A1,A2,A3,A4,A5,A6,A7,sundir[0],sundir[1],sundir[2],dir,0);
			}

			for (j=0 ; j<148 ; j++) {
				if(SkyPatchLuminance[j]>180000)
					SkyPatchLuminance[j]=0;
			}


			//assign Perez Coefficients
			A1=diffnormalization_solar_radiation;	/*A1			- diffus normalization*/
			A2=groundbr_solar_radiation;			/*A2			- ground brightness*/
			A3= c_perez[0];				/*A3            - coefficients for the Perez model*/
			A4= c_perez[1];
			A5= c_perez[2];
			A6= c_perez[3];
			A7= c_perez[4];
			
			for (j=0 ; j<145 ; j++){
				SkyPatchSolarRadiation[j]=horizon_factor[j]*skybright(Dx_dif_patch[j],Dy_dif_patch[j],Dz_dif_patch[j],A1,A2,A3,A4,A5,A6,A7,sundir[0],sundir[1],sundir[2],dir,j);
				if( horizon_factor[j]<1)
					SkyPatchSolarRadiation[j]+=(1-horizon_factor[j])*skybright(0.9961946,0,-0.087156,A1,A2,A3,A4,A5,A6,A7,sundir[0],sundir[1],sundir[2],dir,0);
				if(reduction<1.0)
					SkyPatchSolarRadiation[j]*=reduction;
			}
			// assign ground brightness
			if(dds_file_format) // new dds file format chosen
				{
					/* 0 to 10 */  SkyPatchSolarRadiation[145]=skybright(0.9397,0,-0.342,A1,A2,A3,A4,A5,A6,A7,sundir[0],sundir[1],sundir[2],dir,0);
				}else{
				/* 0 to 10 */  SkyPatchSolarRadiation[145]=skybright(0.9961946,0,-0.087156,A1,A2,A3,A4,A5,A6,A7,sundir[0],sundir[1],sundir[2],dir,0);
				/* 10 to 30*/  SkyPatchSolarRadiation[146]=skybright(0.9397,0,-0.342,A1,A2,A3,A4,A5,A6,A7,sundir[0],sundir[1],sundir[2],dir,0);
				/* 30 to 90 */ SkyPatchSolarRadiation[147]=skybright(0,0,-1.0,A1,A2,A3,A4,A5,A6,A7,sundir[0],sundir[1],sundir[2],dir,0);
			}

			for (j=0 ; j<148 ; j++) {
				if(SkyPatchSolarRadiation[j]>1700)
					SkyPatchSolarRadiation[j]=0;
			}
			
			
			
	return 0;
}





double
normsc(double altitude,int       S_INTER)			/* compute normalization factor (E0*F2/L0) */
{
	static double  nfc[2][5] = {
		/* clear sky approx. */
		{2.766521, 0.547665, -0.369832, 0.009237, 0.059229},
		/* intermediate sky approx. */
		{3.5556, -2.7152, -1.3081, 1.0660, 0.60227},
	};
	register double  *nf;
	double  x, nsc;
	register int  i;
	/* polynomial approximation */
	nf = nfc[S_INTER];
	x = (altitude - M_PI/4.0)/(M_PI/4.0);
	nsc = nf[i=4];
	while (i--)
		nsc = nsc*x + nf[i];

	return(nsc);
}





/*=============================*/
/*=== direct contribution =====*/
/*=============================*/

int write_segments_direct(double dir,double dif, int *number_direct_coefficients, int *shadow_testing_on, int count_var,int current_shadow_testing_value)
{
	static int number145[8]= { 0, 30, 60, 84, 108, 126, 138, 144 };
	static double ring_division145[8]= { 30.0, 30.0, 24.0, 24.0, 18.0, 12.0, 6.0, 0.0 };
	static int number2305[29]= { 0,120,240,360,480,600,720,840,960,1056,1152,1248,1344,1440,1536,1632,1728,1800,1872,1944,2016,2064,2112,2160,2208,2232,2256,2280,2304 };
	static double ring_division2305[29]= { 120.0,120.0,120.0,120.0,120.0,120.0,120.0,120.0,96.0,96.0,96.0,96.0,96.0,96.0,96.0,96.0,72.0,72.0,72.0,72.0,48.0,48.0,48.0,48.0,24.0,24.0,24.0,24.0,0.0 };




	int ringnumber;

	int i=0,j=0,h00=0,h01=0,k=0,i_last=0;
	float angle1=0.0,max_angle=0.0, Nx=0.0,Ny=0.0,Nz=0.0;
	int mon1=0, mon0=0,jd0=0,jd1=0, jd=0;
	double solar_time=0.0, sd=0.0;
	int chosen_value1=0, chosen_value2=0, chosen_value3=0, chosen_value4=0;
	float min_diff1=1, min_diff2=1, min_diff3=1, min_diff4=1;
	float azimuth_tmp=0;
	double chosen_time=0.0 ,time_difference=0.0,max_time_difference=0.0, min_alt_difference=0.0;
	double weight1=0.0,weight2=0.0, weight3=0.0,weight4=0.0,sum_weight=0.0;
	double altitude=0.0, azimuth=0.0;
	double altitude1=0.0, azimuth1=0.0,altitude0=0.0, azimuth0=0.0;
	int chosen_value=0, shadow_counter=0;
	double adapted_time0=0, adapted_time1=0;
	int number_of_diffuse_and_ground_dc=148;
	float summe1=0.0;
	float Dx,Dy,Dz;
	int j1=0;
	float *pointer_dc;
	float *pointer_sky= NULL;
	float DirectDirectSkyPatchSolarRadiation=0;
	float DirectDirectSkyPatchLuminance=0;
	float DirectDirectContribution=0;
	float DirectDC[2305][3];
	int base_value=0;

	if(dds_file_format) { //DDS
		number_of_diffuse_and_ground_dc=146;
		dc_coupling_mode=4;
		DirectDC[0][0]=-96.0; DirectDC[0][1]=6.0;
		DirectDC[1][0]=-108.0; DirectDC[1][1]=6.0;
		DirectDC[2][0]=-120.0; DirectDC[2][1]=6.0;
		DirectDC[3][0]=-132.0; DirectDC[3][1]=6.0;
		DirectDC[4][0]=-144.0; DirectDC[4][1]=6.0;
		DirectDC[5][0]=-156.0; DirectDC[5][1]=6.0;
		DirectDC[6][0]=-168.0; DirectDC[6][1]=6.0;
		DirectDC[7][0]=180.0; DirectDC[7][1]=6.0;
		DirectDC[8][0]=168.0; DirectDC[8][1]=6.0;
		DirectDC[9][0]=156.0; DirectDC[9][1]=6.0;
		DirectDC[10][0]=144.0; DirectDC[10][1]=6.0;
		DirectDC[11][0]=132.0; DirectDC[11][1]=6.0;
		DirectDC[12][0]=120.0; DirectDC[12][1]=6.0;
		DirectDC[13][0]=108.0; DirectDC[13][1]=6.0;
		DirectDC[14][0]=96.0; DirectDC[14][1]=6.0;
		DirectDC[15][0]=84.0; DirectDC[15][1]=6.0;
		DirectDC[16][0]=72.0; DirectDC[16][1]=6.0;
		DirectDC[17][0]=60.0; DirectDC[17][1]=6.0;
		DirectDC[18][0]=48.0; DirectDC[18][1]=6.0;
		DirectDC[19][0]=36.0; DirectDC[19][1]=6.0;
		DirectDC[20][0]=24.0; DirectDC[20][1]=6.0;
		DirectDC[21][0]=12.0; DirectDC[21][1]=6.0;
		DirectDC[22][0]=0.0; DirectDC[22][1]=6.0;
		DirectDC[23][0]=-12.0; DirectDC[23][1]=6.0;
		DirectDC[24][0]=-24.0; DirectDC[24][1]=6.0;
		DirectDC[25][0]=-36.0; DirectDC[25][1]=6.0;
		DirectDC[26][0]=-48.0; DirectDC[26][1]=6.0;
		DirectDC[27][0]=-60.0; DirectDC[27][1]=6.0;
		DirectDC[28][0]=-72.0; DirectDC[28][1]=6.0;
		DirectDC[29][0]=-84.0; DirectDC[29][1]=6.0;
		DirectDC[30][0]=-96.0; DirectDC[30][1]=18.0;
		DirectDC[31][0]=-108.0; DirectDC[31][1]=18.0;
		DirectDC[32][0]=-120.0; DirectDC[32][1]=18.0;
		DirectDC[33][0]=-132.0; DirectDC[33][1]=18.0;
		DirectDC[34][0]=-144.0; DirectDC[34][1]=18.0;
		DirectDC[35][0]=-156.0; DirectDC[35][1]=18.0;
		DirectDC[36][0]=-168.0; DirectDC[36][1]=18.0;
		DirectDC[37][0]=180.0; DirectDC[37][1]=18.0;
		DirectDC[38][0]=168.0; DirectDC[38][1]=18.0;
		DirectDC[39][0]=156.0; DirectDC[39][1]=18.0;
		DirectDC[40][0]=144.0; DirectDC[40][1]=18.0;
		DirectDC[41][0]=132.0; DirectDC[41][1]=18.0;
		DirectDC[42][0]=120.0; DirectDC[42][1]=18.0;
		DirectDC[43][0]=108.0; DirectDC[43][1]=18.0;
		DirectDC[44][0]=96.0; DirectDC[44][1]=18.0;
		DirectDC[45][0]=84.0; DirectDC[45][1]=18.0;
		DirectDC[46][0]=72.0; DirectDC[46][1]=18.0;
		DirectDC[47][0]=60.0; DirectDC[47][1]=18.0;
		DirectDC[48][0]=48.0; DirectDC[48][1]=18.0;
		DirectDC[49][0]=36.0; DirectDC[49][1]=18.0;
		DirectDC[50][0]=24.0; DirectDC[50][1]=18.0;
		DirectDC[51][0]=12.0; DirectDC[51][1]=18.0;
		DirectDC[52][0]=0.0; DirectDC[52][1]=18.0;
		DirectDC[53][0]=-12.0; DirectDC[53][1]=18.0;
		DirectDC[54][0]=-24.0; DirectDC[54][1]=18.0;
		DirectDC[55][0]=-36.0; DirectDC[55][1]=18.0;
		DirectDC[56][0]=-48.0; DirectDC[56][1]=18.0;
		DirectDC[57][0]=-60.0; DirectDC[57][1]=18.0;
		DirectDC[58][0]=-72.0; DirectDC[58][1]=18.0;
		DirectDC[59][0]=-84.0; DirectDC[59][1]=18.0;
		DirectDC[60][0]=-97.5; DirectDC[60][1]=30.0;
		DirectDC[61][0]=-112.5; DirectDC[61][1]=30.0;
		DirectDC[62][0]=-127.5; DirectDC[62][1]=30.0;
		DirectDC[63][0]=-142.5; DirectDC[63][1]=30.0;
		DirectDC[64][0]=-157.5; DirectDC[64][1]=30.0;
		DirectDC[65][0]=-172.5; DirectDC[65][1]=30.0;
		DirectDC[66][0]=172.5; DirectDC[66][1]=30.0;
		DirectDC[67][0]=157.5; DirectDC[67][1]=30.0;
		DirectDC[68][0]=142.5; DirectDC[68][1]=30.0;
		DirectDC[69][0]=127.5; DirectDC[69][1]=30.0;
		DirectDC[70][0]=112.5; DirectDC[70][1]=30.0;
		DirectDC[71][0]=97.5; DirectDC[71][1]=30.0;
		DirectDC[72][0]=82.5; DirectDC[72][1]=30.0;
		DirectDC[73][0]=67.5; DirectDC[73][1]=30.0;
		DirectDC[74][0]=52.5; DirectDC[74][1]=30.0;
		DirectDC[75][0]=37.5; DirectDC[75][1]=30.0;
		DirectDC[76][0]=22.5; DirectDC[76][1]=30.0;
		DirectDC[77][0]=7.5; DirectDC[77][1]=30.0;
		DirectDC[78][0]=-7.5; DirectDC[78][1]=30.0;
		DirectDC[79][0]=-22.5; DirectDC[79][1]=30.0;
		DirectDC[80][0]=-37.5; DirectDC[80][1]=30.0;
		DirectDC[81][0]=-52.5; DirectDC[81][1]=30.0;
		DirectDC[82][0]=-67.5; DirectDC[82][1]=30.0;
		DirectDC[83][0]=-82.5; DirectDC[83][1]=30.0;
		DirectDC[84][0]=-97.5; DirectDC[84][1]=42.0;
		DirectDC[85][0]=-112.5; DirectDC[85][1]=42.0;
		DirectDC[86][0]=-127.5; DirectDC[86][1]=42.0;
		DirectDC[87][0]=-142.5; DirectDC[87][1]=42.0;
		DirectDC[88][0]=-157.5; DirectDC[88][1]=42.0;
		DirectDC[89][0]=-172.5; DirectDC[89][1]=42.0;
		DirectDC[90][0]=172.5; DirectDC[90][1]=42.0;
		DirectDC[91][0]=157.5; DirectDC[91][1]=42.0;
		DirectDC[92][0]=142.5; DirectDC[92][1]=42.0;
		DirectDC[93][0]=127.5; DirectDC[93][1]=42.0;
		DirectDC[94][0]=112.5; DirectDC[94][1]=42.0;
		DirectDC[95][0]=97.5; DirectDC[95][1]=42.0;
		DirectDC[96][0]=82.5; DirectDC[96][1]=42.0;
		DirectDC[97][0]=67.5; DirectDC[97][1]=42.0;
		DirectDC[98][0]=52.5; DirectDC[98][1]=42.0;
		DirectDC[99][0]=37.5; DirectDC[99][1]=42.0;
		DirectDC[100][0]=22.5; DirectDC[100][1]=42.0;
		DirectDC[101][0]=7.5; DirectDC[101][1]=42.0;
		DirectDC[102][0]=-7.5; DirectDC[102][1]=42.0;
		DirectDC[103][0]=-22.5; DirectDC[103][1]=42.0;
		DirectDC[104][0]=-37.5; DirectDC[104][1]=42.0;
		DirectDC[105][0]=-52.5; DirectDC[105][1]=42.0;
		DirectDC[106][0]=-67.5; DirectDC[106][1]=42.0;
		DirectDC[107][0]=-82.5; DirectDC[107][1]=42.0;
		DirectDC[108][0]=-100.0; DirectDC[108][1]=54.0;
		DirectDC[109][0]=-120.0; DirectDC[109][1]=54.0;
		DirectDC[110][0]=-140.0; DirectDC[110][1]=54.0;
		DirectDC[111][0]=-160.0; DirectDC[111][1]=54.0;
		DirectDC[112][0]=180.0; DirectDC[112][1]=54.0;
		DirectDC[113][0]=160.0; DirectDC[113][1]=54.0;
		DirectDC[114][0]=140.0; DirectDC[114][1]=54.0;
		DirectDC[115][0]=120.0; DirectDC[115][1]=54.0;
		DirectDC[116][0]=100.0; DirectDC[116][1]=54.0;
		DirectDC[117][0]=80.0; DirectDC[117][1]=54.0;
		DirectDC[118][0]=60.0; DirectDC[118][1]=54.0;
		DirectDC[119][0]=40.0; DirectDC[119][1]=54.0;
		DirectDC[120][0]=20.0; DirectDC[120][1]=54.0;
		DirectDC[121][0]=0.0; DirectDC[121][1]=54.0;
		DirectDC[122][0]=-20.0; DirectDC[122][1]=54.0;
		DirectDC[123][0]=-40.0; DirectDC[123][1]=54.0;
		DirectDC[124][0]=-60.0; DirectDC[124][1]=54.0;
		DirectDC[125][0]=-80.0; DirectDC[125][1]=54.0;
		DirectDC[126][0]=-105.0; DirectDC[126][1]=66.0;
		DirectDC[127][0]=-135.0; DirectDC[127][1]=66.0;
		DirectDC[128][0]=-165.0; DirectDC[128][1]=66.0;
		DirectDC[129][0]=165.0; DirectDC[129][1]=66.0;
		DirectDC[130][0]=135.0; DirectDC[130][1]=66.0;
		DirectDC[131][0]=105.0; DirectDC[131][1]=66.0;
		DirectDC[132][0]=75.0; DirectDC[132][1]=66.0;
		DirectDC[133][0]=45.0; DirectDC[133][1]=66.0;
		DirectDC[134][0]=15.0; DirectDC[134][1]=66.0;
		DirectDC[135][0]=-15.0; DirectDC[135][1]=66.0;
		DirectDC[136][0]=-45.0; DirectDC[136][1]=66.0;
		DirectDC[137][0]=-75.0; DirectDC[137][1]=66.0;
		DirectDC[138][0]=-120.0; DirectDC[138][1]=78.0;
		DirectDC[139][0]=180.0; DirectDC[139][1]=78.0;
		DirectDC[140][0]=120.0; DirectDC[140][1]=78.0;
		DirectDC[141][0]=60.0; DirectDC[141][1]=78.0;
		DirectDC[142][0]=0.0; DirectDC[142][1]=78.0;
		DirectDC[143][0]=-60.0; DirectDC[143][1]=78.0;
		DirectDC[144][0]=0.0; DirectDC[144][1]=90.0;

		for (j=0 ; j<145;j++) {
			DirectDC[j][2]=DirectDC[j][0];
			if(DirectDC[j][0]<=-90)
				DirectDC[j][0]=(-1)*(DirectDC[j][0]+90.0);
			else
				DirectDC[j][0]=(270.0-DirectDC[j][0]);
			DirectDC[j][0]*=0.017453292;
		}
	}

	if( dir<=dir_threshold ) {	//discard direct contribution
		for (k=0 ; k < TotalNumberOfDCFiles; k++) {
			dc_shading_coeff_rewind( k );

			for (j=0 ; j<number_of_sensors ; j++) {
//#ifndef PROCESS_ROW
//#else
//			struct dc_shading_coeff_data_s* dcd= dc_shading_coeff_read( k );
//#endif


				if(sensor_unit[j]==0 || sensor_unit[j]==1)
					pointer_sky = &SkyPatchLuminance[0];
				if(sensor_unit[j]==2 || sensor_unit[j]==3 )
					pointer_sky = &SkyPatchSolarRadiation[0];

				summe1=0;
				i=0;
//#ifndef PROCESS_ROW
				pointer_dc = dc_shading[k][j];
//#else
//				pointer_dc= dcd->dc;
//#endif
				while(i<number_of_diffuse_and_ground_dc){
					summe1+= (*pointer_dc) * (*pointer_sky);
//#ifndef PROCESS_ROW
					pointer_dc+=dc_shading_next[k][j][i]-i;
					pointer_sky+=dc_shading_next[k][j][i]-i;
					i= dc_shading_next[k][j][i];
//#else
//					pointer_dc+= dcd->next[i] - i;
//					pointer_sky+= dcd->next[i] - i;
//					i= dcd->next[i];
//#endif
				}
				// simplified blinds model
				if(simple_blinds_model==1 && k ==1 )
					summe1*=0.25;
				if(sensor_unit[j]==0 || sensor_unit[j]==1)	
					fprintf(SHADING_ILLUMINANCE_FILE[k]," %.0f",summe1);
				if(sensor_unit[j]==2 || sensor_unit[j]==3 )	
					fprintf(SHADING_ILLUMINANCE_FILE[k]," %.2f",summe1);
			}
		}

		/*reset SkyPatchLuminance & SkyPatchSolarRadiation */
		for (i=0 ; i< number_of_diffuse_and_ground_dc; i++)
			{
				SkyPatchLuminance[i]=0;
				SkyPatchSolarRadiation[i]=0;
			}


	}else{
		jd= jdate(month, day);
		sd=sdec(jd);
		solar_time=hour+stadj(jdate(month, day));
		altitude =(180.0/M_PI)* salt( sd,solar_time);
		azimuth = (180.0/M_PI)*sazi(sd,solar_time);


		switch ( dc_coupling_mode) {
			/* interpolated among 4 nearest direct coefficients =0*/
			/* interpolation with shadow testing =2 */
		case 0 : case 2  : {
			/* regular or advanced interpolation*/

			//===========================================
			// calculate weights of four nearest patches
			//===========================================

			// Step 1: find the four nearest patches
			//======================================
			if (jd >= 355 || jd < 52) {			/*Dec Feb*/
				mon0=12; mon1=2;jd0=355;jd1=52;}
			if (jd >= 52 && jd < 80) { 			/* Feb Mar*/
				mon0=2; mon1=3;jd0=52;jd1=80;}
			if (jd >= 80 && jd < 111) { 			/*Mar Apr*/
				mon0=3; mon1=4;jd0=80;jd1=111;}
			if (jd >= 111 && jd < 172) { 			/*Apr Jun*/
				mon0=4; mon1=6;jd0=111;jd1=172;}
			if (jd >= 172 && jd < 233) {			 /*Jun Aug*/
				mon0=4; mon1=6;jd1=172;jd0=233;}
			if (jd >= 233 && jd < 264) { 			/*Aug Sep*/
				mon0=3; mon1=4;jd1=233;jd0=264;}
			if (jd >= 264 && jd < 294) { 			/*Sep Oct*/
				mon0=2; mon1=3;jd1=264;jd0=294;}
			if (jd >= 294 && jd < 355) { 			 /*Oct Dec*/
				mon0=12; mon1=2;jd1=294;jd0=355;}

			altitude0 =(180.0/M_PI)* salt( sdec(jd0),solar_time);
			azimuth0 = (180.0/M_PI)*sazi(sdec(jd0),solar_time);
			altitude1 =(180.0/M_PI)* salt( sdec(jd1),solar_time);
			azimuth1 = (180.0/M_PI)*sazi(sdec(jd1),solar_time);

			/* find corresponding time */
			adapted_time0=hour+stadj(jdate(mon0, 21));
			adapted_time1=hour+stadj(jdate(mon1, 21));
			for (j=1 ; j<25 ; j++){
				if ( adapted_time0 > direct_calendar[mon0][j][0] && adapted_time0 < direct_calendar[mon0][j+1][0] ){h00=j;j=25;}
				/*printf("adapted %f %f\n",adapted_time,direct_calendar[mon0][j+1][0]);*/
				if ( j == 24 ){ fprintf(stdout,"ds_illum: fatal error - loop in gendaylit_algorithm failed\n");exit(1);}
			}
			for (j=1 ; j<25 ; j++){
				if ( adapted_time1 > direct_calendar[mon1][j][0] && adapted_time1 < direct_calendar[mon1][j+1][0] ){h01=j;j=25;}
				if ( j == 24 ){ fprintf(stdout,"ds_illum: fatal error - loop in gendaylit_algorithm failed\n");exit (1);}
			}

			Dx=cos((0.017453292)*azimuth)*cos((0.017453292)*(altitude));
			Dy=sin((0.017453292)*azimuth)*cos((0.017453292)*(altitude));
			Dz=sin((0.017453292)*(altitude));

			// Step 2: assign weights
			//======================================
			weight1=0.0;
			weight2=0.0;
			weight3=0.0;
			weight4=0.0;

			// consider only patches with a positive altitude
			shadow_counter=0;
			if(direct_calendar[mon0][h00][1]>=0.0)
				weight1=1.0;
			if(direct_calendar[mon0][h00+1][1]>=0.0)
				weight2=1.0;
			if(direct_calendar[mon1][h01][1]>=0.0)
				weight3=1.0;
			if(direct_calendar[mon1][h01+1][1]>=0.0)
				weight4=1.0;

			// in shadow testing activated take out those patches that
			// a have a different status than the current sun position
			if( *shadow_testing_on){
				if(current_shadow_testing_value != direct_calendar[mon0][h00][3])
					{
						fprintf(stdout,"weight 1 set to 0 (%d date: %d %d %.3f)\n",current_shadow_testing_value, month,day,hour);
						weight1=0.0;
					}
				if(current_shadow_testing_value != direct_calendar[mon0][h00+1][3])
					{
						fprintf(stdout,"weight 2 set to 0 (%d date: %d %d %.3f)\n",current_shadow_testing_value, month,day,hour);
						weight2=0.0;
					}
				if(current_shadow_testing_value != direct_calendar[mon1][h01][3])
					{
						fprintf(stdout,"weight 3 set to 0 (%d date: %d %d %.3f)\n",current_shadow_testing_value, month,day,hour);
						weight3=0.0;
					}
				if(current_shadow_testing_value != direct_calendar[mon1][h01+1][3])
					{
						fprintf(stdout,"weight 4 set to 0 (%d date: %d %d %.3f)\n",current_shadow_testing_value, month,day,hour);
						weight4=0.0;
					}
			}

			//assign the weight according to the distance of the current patch from the
			// individual four patches
			weight1*=(altitude1-altitude)/(altitude1-altitude0);
			weight2*=(altitude1-altitude)/(altitude1-altitude0);
			weight3*=(altitude-altitude0)/(altitude1-altitude0);
			weight4*=(altitude-altitude0)/(altitude1-altitude0);
			weight1*=((direct_calendar[mon0][h00+1][0]-adapted_time0)/(direct_calendar[mon0][h00+1][0]-direct_calendar[mon0][h00][0]));
			weight2*=((adapted_time0-direct_calendar[mon0][h00][0])/(direct_calendar[mon0][h00+1][0]-direct_calendar[mon0][h00][0]));
			weight3*=((direct_calendar[mon1][h01+1][0]-adapted_time1)/(direct_calendar[mon1][h01+1][0]-direct_calendar[mon1][h01][0]));
			weight4*=((adapted_time1-direct_calendar[mon1][h01][0])/(direct_calendar[mon1][h01+1][0]-direct_calendar[mon1][h01][0]));

			// normalize weights
			sum_weight=weight1+weight2+weight3+weight4;
			if(sum_weight>0){
				weight1=weight1/sum_weight;
				weight2=weight2/sum_weight;
				weight3=weight3/sum_weight;
				weight4=weight4/sum_weight;
			}else{
				if( *shadow_testing_on)
					{
						fprintf(stdout,"ds_illum: warning shadow testing - all 4 nearest patches have a different status than the current sun position (%d date: %d %d %.3f)\n",current_shadow_testing_value, month,day,hour);
						fprintf(stdout,"          direct sun contribution turned off\n");
					}
				weight1=0.0;
				weight2=0.0;
				weight3=0.0;
				weight4=0.0;
			}

			//====================================================
			//combine direct irradiance with daylight coefficients
			//====================================================
			j1=0;
			i=12;

			if ( ( i==mon0 ) || ( i==mon1 )){
				if ( i==mon0 ) {
					for (j=1 ; j<h00 ; j++){
						if(direct_calendar[i][j][1]>0)
							j1++;
					}
					if (direct_calendar[i][h00][1]>0){
						j1++;
						SkyPatchLuminance[147+j1]=(solarradiance_luminance/1.0)*(weight1);
						SkyPatchSolarRadiation[147+j1]=(solarradiance_solar_radiation/1.0)*(weight1);
					}
					if (direct_calendar[i][h00+1][1]>0){
						j1++;
						SkyPatchLuminance[147+j1]=(solarradiance_luminance/1.0)*(weight2);
						SkyPatchSolarRadiation[147+j1]=(solarradiance_solar_radiation/1.0)*(weight2);
					}
					for (j=h00+2 ; j<25 ; j++){
						if(direct_calendar[i][j][1]>0)
							j1++;
					}
				}
				if ( i==mon1 ) {
					for (j=1 ; j<h01 ; j++){
						if(direct_calendar[i][j][1]>0){
							j1++;
						}
					}
					if (direct_calendar[i][h01][1]>0){
						j1++;
						SkyPatchLuminance[147+j1]=(solarradiance_luminance/1.0)*(weight3);
						SkyPatchSolarRadiation[147+j1]=(solarradiance_solar_radiation/1.0)*(weight3);
					}
					if (direct_calendar[i][h01+1][1]>0){
						j1++;
						SkyPatchLuminance[147+j1]=(solarradiance_luminance/1.0)*(weight4);
						SkyPatchSolarRadiation[147+j1]=(solarradiance_solar_radiation/1.0)*(weight4);
					}
					for (j=h01+2 ; j<25 ; j++){
						if(direct_calendar[i][j][1]>0){
							j1++;
						}
					}
				}
			}else{ // not ( i==mon0 ) || ( i==mon1 )
				for (j=1 ; j<25 ; j++){
					if(direct_calendar[i][j][1]>0)
						j1++;
				}
			}

			for (i=1 ; i<12 ; i++){
				if ( ( i==mon0 ) || ( i==mon1 )){
					if ( i==mon0 ) {
						for (j=1 ; j<h00 ; j++){
							if(direct_calendar[i][j][1]>0)
								j1++;
						}
						if (direct_calendar[i][h00][1]>0){
							j1++;
							SkyPatchLuminance[147+j1]=(solarradiance_luminance/1.0)*(weight1);
							SkyPatchSolarRadiation[147+j1]=(solarradiance_solar_radiation/1.0)*(weight1);
						}
						if (direct_calendar[i][h00+1][1]>0){
							j1++;
							SkyPatchLuminance[147+j1]=(solarradiance_luminance/1.0)*(weight2);
							SkyPatchSolarRadiation[147+j1]=(solarradiance_solar_radiation/1.0)*(weight2);
						}
						for (j=h00+2 ; j<25 ; j++){
							if(direct_calendar[i][j][1]>0){
								j1++;
							}
						}
					}
					if ( i==mon1 ) {
						for (j=1 ; j<h01 ; j++){
							if(direct_calendar[i][j][1]>0){
								j1++;
							}
						}
						if (direct_calendar[i][h01][1]>0){
							j1++;
							SkyPatchLuminance[147+j1]=(solarradiance_luminance/1.0)*(weight3);
							SkyPatchSolarRadiation[147+j1]=(solarradiance_solar_radiation/1.0)*(weight3);
						}
						if (direct_calendar[i][h01+1][1]>0){
							j1++;
							SkyPatchLuminance[147+j1]=(solarradiance_luminance/1.0)*(weight4);
							SkyPatchSolarRadiation[147+j1]=(solarradiance_solar_radiation/1.0)*(weight4);
						}
						for (j=h01+2 ; j<25 ; j++){
							if(direct_calendar[i][j][1]>0)
								j1++;
						}
					}
				}else{
					for (j=1 ; j<25 ; j++){
						if(direct_calendar[i][j][1]>0)
							j1++;
					}
				}

			}
			break;}
		case 1 :{ /* nearest neighbor*/
			chosen_time=0; time_difference=0;
			max_time_difference=12; min_alt_difference=90;
			max_angle=2*M_PI;
			Dx=cos((0.017453292)*azimuth)*cos((0.017453292)*(altitude));
			Dy=sin((0.017453292)*azimuth)*cos((0.017453292)*(altitude));
			Dz=sin((0.017453292)*(altitude));
			for (j=0 ; j< *number_direct_coefficients; j++){
				Nx=cos((0.017453292)*direct_pts[j][2])*cos((0.017453292)*(direct_pts[j][1]));
				Ny=sin((0.017453292)*direct_pts[j][2])*cos((0.017453292)*(direct_pts[j][1]));
				Nz=sin((0.017453292)*(direct_pts[j][1]));
				angle1=acos(Nx*Dx+Ny*Dy+Nz*Dz);
				if (angle1 < max_angle ){
					if( *shadow_testing_on){
						if( (direct_view[count_var] -direct_pts[j][3]) ==0 ){max_angle=angle1;chosen_value=j;}
					}else{max_angle=angle1;chosen_value=j;}
				}
			}
			if(max_angle==2*M_PI) {
				fprintf(stdout,"ds_illum: warning -  shadow test of point at %d %d %f does not correspond to any direct daylight coefficient\n", month, day, hour );
			}
			SkyPatchLuminance[148+chosen_value]=(solarradiance_luminance/1.0);
			SkyPatchSolarRadiation[148+chosen_value]=(solarradiance_solar_radiation/1.0);
			break;
		}
		case 3 : { /* no direct contribution*/
			break;
		}
		case 4 : { /* new dds file format*/

			//find four nearest direct-indirect sun postions
			//++++++++++++++++++++++++++++++++++++++++++++++
			chosen_value1=0;
			chosen_value2=0;
			chosen_value3=0;
			chosen_value4=0;
			min_diff1=0;
			min_diff2=0;
			min_diff3=0;
			min_diff4=0;

			if(azimuth<=-90)
				azimuth_tmp=(-1)*(azimuth+90.0);
			else
				azimuth_tmp=(270.0-azimuth);

			azimuth_tmp*=0.017453292;

			Dx=cos(azimuth_tmp)*cos((0.017453292)*(altitude));
			Dy=sin(azimuth_tmp)*cos((0.017453292)*(altitude));
			Dz=sin((0.017453292)*(altitude));

			//determine four surrounding direct-indirect DC coordinates:

			if(altitude<=6.0) //interpolate between two DC
			{
				for (j=0 ; j<30;j++)
				{
					if((fabs(azimuth-DirectDC[j][2])<12.0) &&((fabs(azimuth-DirectDC[j+1][2])<=12.0)||(fabs(azimuth-DirectDC[j][2]-360.0)<=12.0)))
					{
						chosen_value1=j;
						chosen_value2=j+1;
						if(chosen_value2==30)
							chosen_value2=0;
						j=30;
					}
				}

				chosen_value3=40;//assign bogis value and set weight to zero
				chosen_value4=41;//assign bogis value and set weight to zero

				weight1=1.0;
				weight2=1.0;
				weight3=0.0;
				weight4=0.0;


			}

			//second band 6 Deg to 18 Deg
			//===========================
			if(altitude>6.0 && altitude <=18.0)
			{
				//get two direct-indirect DC in first band
				base_value=0;
				for (j=0 ; j<30;j++)
				{
					if((fabs(azimuth-DirectDC[j+base_value][2])<12.0) &&((fabs(azimuth-DirectDC[j+1+base_value][2])<=12.0)||(fabs(azimuth-DirectDC[j+base_value][2]-360.0)<=12.0)))
					{
						chosen_value1=base_value+j;
						chosen_value2=base_value+j+1;
						if(chosen_value2==base_value+30)
							chosen_value2=base_value;
						j=30;
					}
				}
				if(chosen_value1==0)
				{
					chosen_value1=base_value+29;
					chosen_value2=base_value;
				}

				//get two direct-indirect DC in second band
				base_value=30;
				for (j=0 ; j<30;j++)
				{
					if((fabs(azimuth-DirectDC[j+base_value][2])<12.0) &&((fabs(azimuth-DirectDC[j+1+base_value][2])<=12.0)||(fabs(azimuth-DirectDC[j+base_value][2]-360.0)<=12.0)))
					{
						chosen_value3=base_value+j;
						chosen_value4=base_value+j+1;
						if(chosen_value4==base_value+30)
							chosen_value4=base_value;
						j=30;
					}
				}
				if(chosen_value3==0)
				{
					chosen_value3=base_value+29;
					chosen_value4=base_value;
				}

				weight1=1.0;
				weight2=1.0;
				weight3=1.0;
				weight4=1.0;
			}

			//third band 18 Deg to 30 Deg
			//===========================
			if(altitude>18.0 && altitude <=30.0)
			{
				//get two direct-indirect DC in first band
				base_value=30;
				for (j=0 ; j<30;j++)
				{
					if((fabs(azimuth-DirectDC[j+base_value][2])<12.0) &&((fabs(azimuth-DirectDC[j+1+base_value][2])<=12.0)||(fabs(azimuth-DirectDC[j+base_value][2]-360.0)<=12.0)))
					{
						chosen_value1=base_value+j;
						chosen_value2=base_value+j+1;
						if(chosen_value2==base_value+30)
							chosen_value2=base_value;
						j=30;
					}
				}
				if(chosen_value1==0)
				{
					chosen_value1=base_value+29;
					chosen_value2=base_value;
				}

				//get two direct-indirect DC in second band
				base_value=60;
				for (j=0 ; j<24;j++)
				{
					if((fabs(azimuth-DirectDC[j+base_value][2])<15.0) &&((fabs(azimuth-DirectDC[j+1+base_value][2])<=15.0)||(fabs(azimuth-DirectDC[j+base_value][2]-360.0)<=15.0)))
					{
						chosen_value3=base_value+j;
						chosen_value4=base_value+j+1;
						if(chosen_value4==base_value+24)
							chosen_value4=base_value;
						j=24;
					}
				}
				if(chosen_value3==0)
				{
					chosen_value3=base_value+23;
					chosen_value4=base_value;
				}

				weight1=1.0;
				weight2=1.0;
				weight3=1.0;
				weight4=1.0;
			}


			//forth band 30 Deg to 42 Deg
			//===========================
			if(altitude>30.0 && altitude <=42.0)
			{
				//get two direct-indirect DC in first band
				base_value=60;
				for (j=0 ; j<24;j++)
				{
					if((fabs(azimuth-DirectDC[j+base_value][2])<15.0) &&((fabs(azimuth-DirectDC[j+1+base_value][2])<=15.0)||(fabs(azimuth-DirectDC[j+base_value][2]-360.0)<=15.0)))
					{
						chosen_value1=base_value+j;
						chosen_value2=base_value+j+1;
						if(chosen_value2==base_value+24)
							chosen_value2=base_value;
						j=24;
					}
				}
				if(chosen_value1==0)
				{
					chosen_value1=base_value+23;
					chosen_value2=base_value;
				}


				//get two direct-indirect DC in second band
				base_value=84;
				for (j=0 ; j<24;j++)
				{
					if((fabs(azimuth-DirectDC[j+base_value][2])<15.0) &&((fabs(azimuth-DirectDC[j+1+base_value][2])<=15.0)||(fabs(azimuth-DirectDC[j+base_value][2]-360.0)<=15.0)))
					{
						chosen_value3=base_value+j;
						chosen_value4=base_value+j+1;
						if(chosen_value4==base_value+24)
							chosen_value4=base_value;
						j=24;
					}
				}
				if(chosen_value3==0)
				{
					chosen_value3=base_value+23;
					chosen_value4=base_value;
				}

				weight1=1.0;
				weight2=1.0;
				weight3=1.0;
				weight4=1.0;
			}

			//fifth band 42 Deg to 54 Deg
			//===========================
			if(altitude>42.0 && altitude <=54.0)
			{
				//get two direct-indirect DC in first band
				base_value=84;
				for (j=0 ; j<24;j++)
				{
					if((fabs(azimuth-DirectDC[j+base_value][2])<15.0) &&((fabs(azimuth-DirectDC[j+1+base_value][2])<=15.0)||(fabs(azimuth-DirectDC[j+base_value][2]-360.0)<=15.0)))
					{
						chosen_value1=base_value+j;
						chosen_value2=base_value+j+1;
						if(chosen_value2==base_value+24)
							chosen_value2=base_value;
						j=24;
					}
				}
				if(chosen_value1==0)
				{
					chosen_value1=base_value+23;
					chosen_value2=base_value;
				}

				//get two direct-indirect DC in second band
				base_value=108;
				for (j=0 ; j<18;j++)
				{
					if((fabs(azimuth-DirectDC[j+base_value][2])<20.0) &&((fabs(azimuth-DirectDC[j+1+base_value][2])<=20.0)||(fabs(azimuth-DirectDC[j+base_value][2]-360.0)<=20.0)))
					{
						chosen_value3=base_value+j;
						chosen_value4=base_value+j+1;
						if(chosen_value4==base_value+18)
							chosen_value4=base_value;
						j=18;
					}
				}
				if(chosen_value3==0)
				{
					chosen_value3=base_value+17;
					chosen_value4=base_value;
				}

				weight1=1.0;
				weight2=1.0;
				weight3=1.0;
				weight4=1.0;
			}


			//sixth band 54 Deg to 66 Deg
			//===========================
			if(altitude>54.0 && altitude <=66.0)
			{
				//get two direct-indirect DC in first band
				base_value=108;
				for (j=0 ; j<18;j++)
				{
					if((fabs(azimuth-DirectDC[j+base_value][2])<20.0) &&((fabs(azimuth-DirectDC[j+1+base_value][2])<=20.0)||(fabs(azimuth-DirectDC[j+base_value][2]-360.0)<=20.0)))
					{
						chosen_value1=base_value+j;
						chosen_value2=base_value+j+1;
						if(chosen_value2==base_value+18)
							chosen_value2=base_value;

						j=18;
					}
				}
				if(chosen_value1==0)
				{
					chosen_value1=base_value+17;
					chosen_value2=base_value;
				}

				//get two direct-indirect DC in second band
				base_value=126;
				for (j=0 ; j<12;j++)
				{
					if((fabs(azimuth-DirectDC[j+base_value][2])<30.0) &&((fabs(azimuth-DirectDC[j+1+base_value][2])<=30.0)||(fabs(azimuth-DirectDC[j+base_value][2]-360.0)<=30.0)))
					{
						chosen_value3=base_value+j;
						chosen_value4=base_value+j+1;
						if(chosen_value4==base_value+12)
							chosen_value4=base_value;
						j=12;
					}
				}
				if(chosen_value3==0)
				{
					chosen_value3=base_value+11;
					chosen_value4=base_value;
				}
				weight1=1.0;
				weight2=1.0;
				weight3=1.0;
				weight4=1.0;
			}


			//seventh band 66 Deg to 78 Deg
			//===========================
			if(altitude>66.0 && altitude <=78.0)
			{
				//get two direct-indirect DC in first band
				base_value=126;
				for (j=0 ; j<12;j++)
				{
					if((fabs(azimuth-DirectDC[j+base_value][2])<30.0) &&((fabs(azimuth-DirectDC[j+1+base_value][2])<=30.0)||(fabs(azimuth-DirectDC[j+base_value][2]-360.0)<=30.0)))
					{
						chosen_value1=base_value+j;
						chosen_value2=base_value+j+1;
						if(chosen_value2==base_value+12)
							chosen_value2=base_value;
						j=12;
					}
				}
				if(chosen_value1==0)
				{
					chosen_value1=base_value+11;
					chosen_value2=base_value;
				}

				//get two direct-indirect DC in second band
				base_value=138;
				for (j=0 ; j<6;j++)
				{
					if((fabs(azimuth-DirectDC[j+base_value][2])<60.0) &&((fabs(azimuth-DirectDC[j+1+base_value][2])<=60.0)||(fabs(azimuth-DirectDC[j+base_value][2]-360.0)<=60.0)))
					{
						chosen_value3=base_value+j;
						chosen_value4=base_value+j+1;
						if(chosen_value4==base_value+6)
							chosen_value4=base_value;
						j=6;
					}
				}
				if(chosen_value3==0)
				{
					chosen_value3=base_value+5;
					chosen_value4=base_value;
				}

				weight1=1.0;
				weight2=1.0;
				weight3=1.0;
				weight4=1.0;
			}

			//eighth band 78 Deg to 90 Deg
			//===========================
			if(altitude>78.0 && altitude <=90.0)
			{
				//get two direct-indirect DC in first band
				base_value=138;
				for (j=0 ; j<6;j++)
				{
					if((fabs(azimuth-DirectDC[j+base_value][2])<60.0) &&((fabs(azimuth-DirectDC[j+1+base_value][2])<=60.0)||(fabs(azimuth-DirectDC[j+base_value][2]-360.0)<=60.0)))
					{
						chosen_value1=base_value+j;
						chosen_value2=base_value+j+1;
						if(chosen_value2==base_value+6)
							chosen_value2=0;
						j=6;
					}
				}

				//get two direct-indirect DC in second band
				chosen_value3=144;
				chosen_value4=1;

				if(altitude ==90.0)
				{
					weight3=1.0;
					weight1=0.0;
					weight2=0.0;
					weight4=0.0;
				} else{
					weight1=1.0;
					weight2=1.0;
					weight3=1.0;
					weight4=0.0;
				}
			}


			//++++++++++++++++++++++++++
			//assign and normize weights
			//++++++++++++++++++++++++++

			weight1*=fabs((azimuth-DirectDC[chosen_value2][2])/(DirectDC[chosen_value1][2]-DirectDC[chosen_value2][2]));
			weight2*=fabs((azimuth-DirectDC[chosen_value1][2])/(DirectDC[chosen_value1][2]-DirectDC[chosen_value2][2]));
			weight3*=fabs((azimuth-DirectDC[chosen_value4][2])/(DirectDC[chosen_value3][2]-DirectDC[chosen_value4][2]));
			weight4*=fabs((azimuth-DirectDC[chosen_value3][2])/(DirectDC[chosen_value3][2]-DirectDC[chosen_value4][2]));

			weight1*=(DirectDC[chosen_value3][1]-altitude)/(DirectDC[chosen_value3][1]-DirectDC[chosen_value1][1]);
			weight2*=(DirectDC[chosen_value3][1]-altitude)/(DirectDC[chosen_value3][1]-DirectDC[chosen_value1][1]);
			weight3*=(altitude-DirectDC[chosen_value1][1])/(DirectDC[chosen_value3][1]-DirectDC[chosen_value1][1]);
			weight4*=(altitude-DirectDC[chosen_value1][1])/(DirectDC[chosen_value3][1]-DirectDC[chosen_value1][1]);


			sum_weight=weight1+weight2+weight3+weight4;
			weight1=weight1/sum_weight;
			weight2=weight2/sum_weight;
			weight3=weight3/sum_weight;
			weight4=weight4/sum_weight;


			chosen_value1++;
			chosen_value2++;
			chosen_value3++;
			chosen_value4++;



			SkyPatchLuminance[145+chosen_value1]=(solarradiance_luminance/1.0)*weight1;
			SkyPatchLuminance[145+chosen_value2]=(solarradiance_luminance/1.0)*weight2;
			SkyPatchLuminance[145+chosen_value3]=(solarradiance_luminance/1.0)*weight3;
			SkyPatchLuminance[145+chosen_value4]=(solarradiance_luminance/1.0)*weight4;

			SkyPatchSolarRadiation[145+chosen_value1]=(solarradiance_solar_radiation/1.0)*weight1;
			SkyPatchSolarRadiation[145+chosen_value2]=(solarradiance_solar_radiation/1.0)*weight2;
			SkyPatchSolarRadiation[145+chosen_value3]=(solarradiance_solar_radiation/1.0)*weight3;
			SkyPatchSolarRadiation[145+chosen_value4]=(solarradiance_solar_radiation/1.0)*weight4;
			

			// Step 2: Identify in which patch the sun currently is for the direct-direct DC
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			ringnumber=(int)(1.0*altitude/3.157895);
			if((azimuth>-90) && (azimuth < 180))
				{
					chosen_value= number2305[ringnumber] + (int)((270.0-azimuth)/(360.0/ring_division2305[ringnumber]));
				}else{
				chosen_value= number2305[ringnumber] + (int)((-90.0-azimuth)/(360.0/ring_division2305[ringnumber]));
			}

			//printf(" ringnumber %d  chosen_value 2305: %d\n",ringnumber, chosen_value+290);

			SkyPatchLuminance[290+chosen_value]=(solarradiance_luminance/1.0);
			DirectDirectSkyPatchLuminance=solarradiance_luminance/1.0;
			
			SkyPatchSolarRadiation[290+chosen_value]=(solarradiance_solar_radiation/1.0);
			DirectDirectSkyPatchSolarRadiation=solarradiance_solar_radiation/1.0;
			
			break;
		}
		case 5 : { /* new dds file format with nearest neighbor for direct-indirect and direct-direct*/

			// Step 1: Identify in which patch the sun currently is for the direct-indirect DC
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			ringnumber=(int)(1.0*altitude/12.0);
			// according to Tregenza, the celestial hemisphere is divided into 7 bands and
			// the zenith patch. The bands range from:
			//												altitude center
			// Band 1		0 to 12 Deg			30 patches	6
			// Band 2		12 to 24 Deg		30 patches	18
			// Band 3		24 to 36 Deg		24 patches	30
			// Band 4		36 to 48 Deg		24 patches	42
			// Band 5		48 to 60 Deg		18 patches	54
			// Band 6		60 to 72 Deg		12 patches	66
			// Band 7		72 to 84 Deg		 6 patches	78
			// Band 8		84 to 90 Deg		 1 patche	90
			// since the zenith patch is only takes 6Deg instead of 12, the arc length
			// between 0 and 90 Deg (equlas 0 and Pi/2) is divided into 7.5 units:
			// Therefore, 7.5 units = (int) asin(z=1)/(Pi/2)
			//				1 unit = asin(z)*(2*7.5)/Pi)
			//				1 unit = asin(z)*(15)/Pi)
			// Note that (int) always rounds to the next lowest integer
			if((azimuth>-90) && (azimuth < 180))
				{
					chosen_value= number145[ringnumber] + (int)((270.0-azimuth)/(360.0/ring_division145[ringnumber]));
				}else{
				chosen_value= number145[ringnumber] + (int)((-90.0-azimuth)/(360.0/ring_division145[ringnumber]));
			}

			SkyPatchLuminance[145+chosen_value]=(solarradiance_luminance/1.0);
			SkyPatchSolarRadiation[145+chosen_value]=(solarradiance_solar_radiation/1.0);


			//printf("dds %d dc.coup %d, date %d %d %.3f altitude %f azimuth %f  chosen_value %d ",dds_file_format,dc_coupling_mode,month, day,hour,altitude, azimuth,chosen_value);

			// Step 2: Identify in which patch the sun currently is for the direct-direct DC
			//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			ringnumber=(int)(1.0*altitude/3.157895);
			if((azimuth>-90) && (azimuth < 180))
				{
					chosen_value= number2305[ringnumber] + (int)((270.0-azimuth)/(360.0/ring_division2305[ringnumber]));
				}else{
				chosen_value= number2305[ringnumber] + (int)((-90.0-azimuth)/(360.0/ring_division2305[ringnumber]));
			}

			//printf(" ringnumber %d  chosen_value 2305: %d\n",ringnumber, chosen_value+290);

			SkyPatchLuminance[290+chosen_value]=(solarradiance_luminance/1.0);
			DirectDirectSkyPatchLuminance=solarradiance_luminance/1.0;
			
			SkyPatchSolarRadiation[290+chosen_value]=(solarradiance_solar_radiation/1.0);
			DirectDirectSkyPatchSolarRadiation=solarradiance_solar_radiation/1.0;

			break;
		}
		} /* end switch */


		/* write out files */
		i_last=number_of_diffuse_and_ground_dc;
			NextNonEmptySkyPatch[number_of_diffuse_and_ground_dc]=(number_of_diffuse_and_ground_dc+ *number_direct_coefficients);
			for (i=number_of_diffuse_and_ground_dc ; i<(number_of_diffuse_and_ground_dc+ *number_direct_coefficients) ; i++) {
				if(i>number_of_diffuse_and_ground_dc && (SkyPatchLuminance[i]>0.0 )){  //|| SkyPatchSolarRadiation[i]>0.0
					NextNonEmptySkyPatch[i_last]=i;
					NextNonEmptySkyPatch[i]=(number_of_diffuse_and_ground_dc+ *number_direct_coefficients);
					i_last=i;
				}
			}

		for (k=0 ; k < TotalNumberOfDCFiles ; k++) {
//#ifndef PROCESS_ROW
//#else
			dc_shading_coeff_rewind( k );
//#endif
			for (j=0 ; j<number_of_sensors ; j++) {


//#ifndef PROCESS_ROW
//#else
//			struct dc_shading_coeff_data_s* dcd= dc_shading_coeff_read( k );
//#endif

				summe1=0;
				i=0;
//#ifndef PROCESS_ROW
				pointer_dc = dc_shading[k][j];
//#else
//				pointer_dc= dcd->dc;
//#endif
				if(sensor_unit[j]==0 || sensor_unit[j]==1)
					{
						pointer_sky = &SkyPatchLuminance[0];
						DirectDirectContribution = DirectDirectSkyPatchLuminance;
					}
				if(sensor_unit[j]==2 || sensor_unit[j]==3 )
					{
						pointer_sky = &SkyPatchSolarRadiation[0];
						DirectDirectContribution = DirectDirectSkyPatchSolarRadiation;
					}

				// simplified blind model:
				// sum over all diffuse and ground dc and mutilpy the sum by 0.25
				if(simple_blinds_model==1 && k ==1 ){
					while(i<number_of_diffuse_and_ground_dc){
						summe1+= (*pointer_dc) * (*pointer_sky);
//#ifndef PROCESS_ROW
						pointer_dc+=dc_shading_next[k][j][i]-i;
						pointer_sky+=dc_shading_next[k][j][i]-i;
						i=dc_shading_next[k][j][i];
//#else
//						pointer_dc+= dcd->next[i] - i;
//						pointer_sky+= dcd->next[i] - i;
//						i= dcd->next[i];
//#endif
					}
					summe1*=.25;
				}else if(dds_file_format==2) { //DDS and SHADOWTESTING
					//*number_direct_coefficients= 145; //count only direct indirect
					while(i< (number_of_diffuse_and_ground_dc+ 145)){
						summe1+= (*pointer_dc) * (*pointer_sky);
						if(i<number_of_diffuse_and_ground_dc){
//#ifndef PROCESS_ROW
							pointer_dc+=dc_shading_next[k][j][i]-i;
							pointer_sky+=dc_shading_next[k][j][i]-i;
							i=dc_shading_next[k][j][i];
//#else
//							pointer_dc+= dcd->next[i] - i;
//							pointer_sky+= dcd->next[i] - i;
//							i= dcd->next[i];
//#endif
							if(i>number_of_diffuse_and_ground_dc){
								pointer_dc+=number_of_diffuse_and_ground_dc-i;
								pointer_sky+=number_of_diffuse_and_ground_dc-i;
								i=number_of_diffuse_and_ground_dc;
							}
						}else{
							pointer_dc+=NextNonEmptySkyPatch[i]-i;
							pointer_sky+=NextNonEmptySkyPatch[i]-i;
							i=NextNonEmptySkyPatch[i];
//#ifndef PROCESS_ROW
							while( i < (number_of_diffuse_and_ground_dc+ *number_direct_coefficients) && dc_shading[k][j][i]==0.0 ) {
//#else
//							while( i < (number_of_diffuse_and_ground_dc+ *number_direct_coefficients) && dcd->dc[i]==0.0 ) {
//#endif
								pointer_dc+=NextNonEmptySkyPatch[i]-i;
								pointer_sky+=NextNonEmptySkyPatch[i]-i;
								i=NextNonEmptySkyPatch[i];
							}
						}
					}
					//now read in the direct direct contribution from the pre-simulation run
					summe1+=1.0*DirectDirectContribution*dc_ab0[j][SkyConditionCounter][k];
				} else { // regular blind model or no blinds
					while(i< (number_of_diffuse_and_ground_dc+ *number_direct_coefficients)){
						summe1+= (*pointer_dc) * (*pointer_sky);
						if(i<number_of_diffuse_and_ground_dc){
//#ifndef PROCESS_ROW
							pointer_dc+=dc_shading_next[k][j][i]-i;
							pointer_sky+=dc_shading_next[k][j][i]-i;
							i=dc_shading_next[k][j][i];
//#else
//							pointer_dc+= dcd->next[i] - i;
//							pointer_sky+= dcd->next[i] - i;
//							i= dcd->next[i];
//#endif
							if(i>number_of_diffuse_and_ground_dc){
								pointer_dc+=number_of_diffuse_and_ground_dc-i;
								pointer_sky+=number_of_diffuse_and_ground_dc-i;
								i=number_of_diffuse_and_ground_dc;
							}
						}else{
							pointer_dc+=NextNonEmptySkyPatch[i]-i;
							pointer_sky+=NextNonEmptySkyPatch[i]-i;
							i=NextNonEmptySkyPatch[i];
//#ifndef PROCESS_ROW
							while( i<(number_of_diffuse_and_ground_dc+ *number_direct_coefficients) && dc_shading[k][j][i]==0.0){
//#else
//							while( i<(number_of_diffuse_and_ground_dc+ *number_direct_coefficients) && dcd->dc[i]==0.0) {
//#endif

								pointer_dc+=NextNonEmptySkyPatch[i]-i;
								pointer_sky+=NextNonEmptySkyPatch[i]-i;
								i=NextNonEmptySkyPatch[i];
							}
						}
					}
				}
				if(sensor_unit[j]==0 || sensor_unit[j]==1)	
					fprintf(SHADING_ILLUMINANCE_FILE[k]," %.0f",summe1);
				if(sensor_unit[j]==2 || sensor_unit[j]==3 )	
					fprintf(SHADING_ILLUMINANCE_FILE[k]," %.2f",summe1);
			}

		}
		SkyConditionCounter++;

		//reset SkyPatchLuminance and SkyPatchSolarRadiation
		for (i=0 ; i< number_of_diffuse_and_ground_dc+ *number_direct_coefficients; i++) 
		{
			SkyPatchLuminance[i]=0;
			SkyPatchSolarRadiation[i]=0;
		}
	} /* end else */

	return 0;
}


/*============================================*/
/* Perez models */
/*============================================*/

/* Perez global horizontal luminous efficacy model */
double glob_h_effi_PEREZ()
{

	double 	value;
	double 	category_bounds[10], a[10], b[10], c[10], d[10];
	int   	category_total_number, category_number=0, i;

	if (skyclearness<skyclearinf || skyclearness>skyclearsup || skybrightness<=skybriginf || skybrightness>skybrigsup){;}

	/* initialize category bounds (clearness index bounds) */

	category_total_number = 8 ; /*changed from 8 , Tito */
	category_bounds[1] = 1;
	category_bounds[2] = 1.065;
	category_bounds[3] = 1.230;
	category_bounds[4] = 1.500;
	category_bounds[5] = 1.950;
	category_bounds[6] = 2.800;
	category_bounds[7] = 4.500;
	category_bounds[8] = 6.200;
	category_bounds[9] = 12.1; /*changed from 12.01 , Tito */

	/* initialize model coefficients */
	a[1] = 96.63;
	a[2] = 107.54;
	a[3] = 98.73;
	a[4] = 92.72;
	a[5] = 86.73;
	a[6] = 88.34;
	a[7] = 78.63;
	a[8] = 99.65;

	b[1] = -0.47;
	b[2] = 0.79;
	b[3] = 0.70;
	b[4] = 0.56;
	b[5] = 0.98;
	b[6] = 1.39;
	b[7] = 1.47;
	b[8] = 1.86;

	c[1] = 11.50;
	c[2] = 1.79;
	c[3] = 4.40;
	c[4] = 8.36;
	c[5] = 7.10;
	c[6] = 6.06;
	c[7] = 4.93;
	c[8] = -4.46;

	d[1] = -9.16;
	d[2] = -1.19;
	d[3] = -6.95;
	d[4] = -8.31;
	d[5] = -10.94;
	d[6] = -7.60;
	d[7] = -11.37;
	d[8] = -3.15;

	for (i=1; i<=category_total_number; i++)
		{
			if ( (skyclearness >= category_bounds[i]) && (skyclearness < category_bounds[i+1]) )
				category_number = i;
		}

	value = a[category_number] + b[category_number]*atm_preci_water  +
	    c[category_number]*cos(sunzenith*M_PI/180) +  d[category_number]*log(skybrightness);

	return(value);
}


/* global horizontal diffuse efficacy model, according to PEREZ */
double glob_h_diffuse_effi_PEREZ()
{
	double 	value;
	double 	category_bounds[10], a[10], b[10], c[10], d[10];
	int   	category_total_number, category_number=0, i;

	if (skyclearness<skyclearinf || skyclearness>skyclearsup || skybrightness<=skybriginf || skybrightness>skybrigsup)
		{/*fprintf(stdout, "Warning : skyclearness or skybrightness out of range ; \n Check your input parameters\n");*/};

	/* initialize category bounds (clearness index bounds) */

	category_total_number = 8;
	category_bounds[1] = 1;
	category_bounds[2] = 1.065;
	category_bounds[3] = 1.230;
	category_bounds[4] = 1.500;
	category_bounds[5] = 1.950;
	category_bounds[6] = 2.800;
	category_bounds[7] = 4.500;
	category_bounds[8] = 6.200;
	category_bounds[9] = 12.1; /*changed from 12.01 , Tito */

	/* initialize model coefficients */
	a[1] = 97.24;
	a[2] = 107.22;
	a[3] = 104.97;
	a[4] = 102.39;
	a[5] = 100.71;
	a[6] = 106.42;
	a[7] = 141.88;
	a[8] = 152.23;

	b[1] = -0.46;
	b[2] = 1.15;
	b[3] = 2.96;
	b[4] = 5.59;
	b[5] = 5.94;
	b[6] = 3.83;
	b[7] = 1.90;
	b[8] = 0.35;

	c[1] = 12.00;
	c[2] = 0.59;
	c[3] = -5.53;
	c[4] = -13.95;
	c[5] = -22.75;
	c[6] = -36.15;
	c[7] = -53.24;
	c[8] = -45.27;

	d[1] = -8.91;
	d[2] = -3.95;
	d[3] = -8.77;
	d[4] = -13.90;
	d[5] = -23.74;
	d[6] = -28.83;
	d[7] = -14.03;
	d[8] = -7.98;

	for (i=1; i<=category_total_number; i++)
		{
			if ( (skyclearness >= category_bounds[i]) && (skyclearness <= category_bounds[i+1]) )
				category_number = i;
		}
	value = a[category_number] + b[category_number]*atm_preci_water  + c[category_number]*cos(sunzenith*M_PI/180) +
	    d[category_number]*log(skybrightness);

	return(value);
}


/* direct normal efficacy model, according to PEREZ */

double direct_n_effi_PEREZ()

{
	double 	value;
	double 	category_bounds[10], a[10], b[10], c[10], d[10];
	int   	category_total_number, category_number=0, i;


	if (skyclearness<skyclearinf || skyclearness>skyclearsup || skybrightness<=skybriginf || skybrightness>skybrigsup)
		{/*fprintf(stdout, "Warning : skyclearness or skybrightness out of range ; \n Check your input parameters\n")*/;}


	/* initialize category bounds (clearness index bounds) */

	category_total_number = 8;

	category_bounds[1] = 1;
	category_bounds[2] = 1.065;
	category_bounds[3] = 1.230;
	category_bounds[4] = 1.500;
	category_bounds[5] = 1.950;
	category_bounds[6] = 2.800;
	category_bounds[7] = 4.500;
	category_bounds[8] = 6.200;
	category_bounds[9] = 12.1;


	/* initialize model coefficients */
	a[1] = 57.20;
	a[2] = 98.99;
	a[3] = 109.83;
	a[4] = 110.34;
	a[5] = 106.36;
	a[6] = 107.19;
	a[7] = 105.75;
	a[8] = 101.18;

	b[1] = -4.55;
	b[2] = -3.46;
	b[3] = -4.90;
	b[4] = -5.84;
	b[5] = -3.97;
	b[6] = -1.25;
	b[7] = 0.77;
	b[8] = 1.58;

	c[1] = -2.98;
	c[2] = -1.21;
	c[3] = -1.71;
	c[4] = -1.99;
	c[5] = -1.75;
	c[6] = -1.51;
	c[7] = -1.26;
	c[8] = -1.10;

	d[1] = 117.12;
	d[2] = 12.38;
	d[3] = -8.81;
	d[4] = -4.56;
	d[5] = -6.16;
	d[6] = -26.73;
	d[7] = -34.44;
	d[8] = -8.29;



	for (i=1; i<=category_total_number; i++)
		{
			if ( (skyclearness >= category_bounds[i]) && (skyclearness <= category_bounds[i+1]) )
				category_number = i;
		}

	value = a[category_number] + b[category_number]*atm_preci_water  + c[category_number]*exp(5.73*sunzenith*M_PI/180 - 5) +  d[category_number]*skybrightness;

	if (value < 0) value = 0;

	return(value);
}


/*check the range of epsilon and delta indexes of the perez parametrization*/
void check_parametrization()
{

	if (skyclearness<skyclearinf || skyclearness>skyclearsup || skybrightness<skybriginf || skybrightness>skybrigsup)
		{
			if (skyclearness<skyclearinf){
				if(all_warnings &&((fabs(hour- 12+12-stadj(jdate(month, day))-solar_sunset(month,day))<0.25)||(fabs(hour-solar_sunset(month,day)-stadj(jdate(month, day)) )<0.25) )){
					fprintf(stdout,"ds_illum: warning - sky clearness (%.1f) below range (%d %d %.3f)\n", skyclearness, month, day, hour);
				}
				skyclearness=skyclearinf;
			}
			if (skyclearness>skyclearsup){
				if(all_warnings &&((fabs(hour- 12+12-stadj(jdate(month, day))-solar_sunset(month,day))<0.25)||(fabs(hour-solar_sunset(month,day)-stadj(jdate(month, day)) )<0.25) )){
					fprintf(stdout,"ds_illum: warning - sky clearness (%.1f) above range (%d %d %.3f)\n", skyclearness, month, day, hour);
				}
				skyclearness=skyclearsup;
			}
			if (skybrightness<skybriginf){
				if(all_warnings &&((fabs(hour- 12+12-stadj(jdate(month, day))-solar_sunset(month,day))<0.25)||(fabs(hour-solar_sunset(month,day)-stadj(jdate(month, day)) )<0.25) )){
					fprintf(stdout,"ds_illum: warning - sky brightness (%.1f) below range (%d %d %.3f)\n", skybrightness, month, day, hour);
				}
				skybrightness=skybriginf;
			}
			if (skybrightness>skybrigsup){
				if(all_warnings &&((fabs(hour- 12+12-stadj(jdate(month, day))-solar_sunset(month,day))<0.25)||(fabs(hour-solar_sunset(month,day)-stadj(jdate(month, day)) )<0.25) )){
					fprintf(stdout,"ds_illum: warning - sky brightness (%.1f) above range (%d %d %.3f)\n", skybrightness, month, day, hour);
				}
				skybrightness=skybrigsup;
			}

		}
	else return;
}


/* likelihood of the direct and diffuse components */
void 	check_illuminances()
{
	if (!( (directilluminance>=0) && (directilluminance<=solar_constant_l*1000) && (diffusilluminance>0) ))
		{
			if(directilluminance > solar_constant_l*1000){
				directilluminance=solar_constant_l*1000;fprintf(stdout,"ds_illum: warning -  direct illuminance set to max at %d %d %.3f\n", month,day,hour);
			}else{directilluminance=0;diffusilluminance=0;}
			/*fprintf(stdout,"direct or diffuse illuminances out of range at %d %d %f\n", month,day,hour);*/
		}
	return;
}


void 	check_irradiances()
{
	if (!( (directirradiance>=0) && (directirradiance<=solar_constant_e) && (diffusirradiance>0) ))
		{
			if(diffusirradiance!=0 && directirradiance!=0){
				fprintf(stdout,"ds_illum: warning - direct or diffuse irradiances out of range\n");
				fprintf(stdout,"date %d %d %f\n dir %f dif %f\n",month,day,hour,directirradiance,diffusirradiance);}
		}
	return;
}



/* Perez sky's brightness */
double sky_brightness()
{
	double value;

	value = diffusirradiance * air_mass() / ( solar_constant_e*get_eccentricity());

	return(value);
}


/* Perez sky's clearness */
double sky_clearness()
{
	double value;
	if(diffusirradiance > 0){
		value = ( (diffusirradiance + directirradiance)/(diffusirradiance) + 1.041*sunzenith*M_PI/180*sunzenith*M_PI/180*sunzenith*M_PI/180 ) / (1 + 1.041*sunzenith*M_PI/180*sunzenith*M_PI/180*sunzenith*M_PI/180) ;
	}else{value=0;}
	return(value);
}



/* diffus horizontal irradiance from Perez sky's brightness */
double diffus_irradiance_from_sky_brightness()
{
	double value;

	value = skybrightness / air_mass() * ( solar_constant_e*get_eccentricity());

	return(value);
}


/* direct normal irradiance from Perez sky's clearness */
double direct_irradiance_from_sky_clearness()
{
	double value;

	value = diffus_irradiance_from_sky_brightness();
	value = value * ( (skyclearness-1) * (1+1.041*sunzenith*M_PI/180*sunzenith*M_PI/180*sunzenith*M_PI/180) );
	return(value);
}


void illu_to_irra_index(void)
{
	double	test1=0.1, test2=0.1;
	int	counter=0;

	diffusirradiance = diffusilluminance*solar_constant_e/(solar_constant_l*1000);
	directirradiance = directilluminance*solar_constant_e/(solar_constant_l*1000);
	skyclearness =  sky_clearness();
	skybrightness = sky_brightness();
	if (skyclearness>12) skyclearness=12;
	if (skybrightness<0.05) skybrightness=0.01;


	while ( ((fabs(diffusirradiance-test1)>10) || (fabs(directirradiance-test2)>10)
			 || skyclearness>skyclearinf || skyclearness<skyclearsup
			 || skybrightness>skybriginf || skybrightness<skybrigsup )
			&& !(counter==5) )
		{
			/*fprintf(stdout, "conversion illuminance into irradiance %lf\t %lf\n", diffusirradiance, directirradiance);*/

			test1=diffusirradiance;
			test2=directirradiance;
			counter++;

			diffusirradiance = diffusilluminance/glob_h_diffuse_effi_PEREZ();
			directirradiance = directilluminance/direct_n_effi_PEREZ();

			skybrightness = sky_brightness();
			skyclearness =  sky_clearness();
			if (skyclearness>12) skyclearness=12;
			if (skybrightness<0.05) skybrightness=0.01;
		}
	return;
}


int lect_coeff_perez(float **coeff_perez)
{
	*(*coeff_perez+0 )=1.352500;	*(*coeff_perez+1 )=-0.257600;
	*(*coeff_perez+2 )=-0.269000;	*(*coeff_perez+3 )=-1.436600;
	*(*coeff_perez+4 )=-0.767000;	*(*coeff_perez+5 )=0.000700;
	*(*coeff_perez+6 )=1.273400;	*(*coeff_perez+7 )=-0.123300;
	*(*coeff_perez+8 )=2.800000;	*(*coeff_perez+9 )=0.600400;
	*(*coeff_perez+10 )=1.237500;	*(*coeff_perez+11 )=1.000000;
	*(*coeff_perez+12 )=1.873400;	*(*coeff_perez+13 )=0.629700;
	*(*coeff_perez+14 )=0.973800;	*(*coeff_perez+15 )=0.280900;
	*(*coeff_perez+16 )=0.035600;	*(*coeff_perez+17 )=-0.124600;
	*(*coeff_perez+18 )=-0.571800;	*(*coeff_perez+19 )=0.993800;
	*(*coeff_perez+20 )=-1.221900;	*(*coeff_perez+21 )=-0.773000;
	*(*coeff_perez+22 )=1.414800;	*(*coeff_perez+23 )=1.101600;
	*(*coeff_perez+24 )=-0.205400;	*(*coeff_perez+25 )=0.036700;
	*(*coeff_perez+26 )=-3.912800;	*(*coeff_perez+27 )=0.915600;
	*(*coeff_perez+28 )=6.975000;	*(*coeff_perez+29 )=0.177400;
	*(*coeff_perez+30 )=6.447700;	*(*coeff_perez+31 )=-0.123900;
	*(*coeff_perez+32 )=-1.579800;	*(*coeff_perez+33 )=-0.508100;
	*(*coeff_perez+34 )=-1.781200;	*(*coeff_perez+35 )=0.108000;
	*(*coeff_perez+36 )=0.262400;	*(*coeff_perez+37 )=0.067200;
	*(*coeff_perez+38 )=-0.219000;	*(*coeff_perez+39 )=-0.428500;
	*(*coeff_perez+40 )=-1.100000;	*(*coeff_perez+41 )=-0.251500;
	*(*coeff_perez+42 )=0.895200;	*(*coeff_perez+43 )=0.015600;
	*(*coeff_perez+44 )=0.278200;	*(*coeff_perez+45 )=-0.181200;
	*(*coeff_perez+46 )=-4.500000;	*(*coeff_perez+47 )=1.176600;
	*(*coeff_perez+48 )=24.721901;	*(*coeff_perez+49 )=-13.081200;
	*(*coeff_perez+50 )=-37.700001;	*(*coeff_perez+51 )=34.843800;
	*(*coeff_perez+52 )=-5.000000;	*(*coeff_perez+53 )=1.521800;
	*(*coeff_perez+54 )=3.922900;	*(*coeff_perez+55 )=-2.620400;
	*(*coeff_perez+56 )=-0.015600;	*(*coeff_perez+57 )=0.159700;
	*(*coeff_perez+58 )=0.419900;	*(*coeff_perez+59 )=-0.556200;
	*(*coeff_perez+60 )=-0.548400;	*(*coeff_perez+61 )=-0.665400;
	*(*coeff_perez+62 )=-0.267200;	*(*coeff_perez+63 )=0.711700;
	*(*coeff_perez+64 )=0.723400;	*(*coeff_perez+65 )=-0.621900;
	*(*coeff_perez+66 )=-5.681200;	*(*coeff_perez+67 )=2.629700;
	*(*coeff_perez+68 )=33.338902;	*(*coeff_perez+69 )=-18.299999;
	*(*coeff_perez+70 )=-62.250000;	*(*coeff_perez+71 )=52.078098;
	*(*coeff_perez+72 )=-3.500000;	*(*coeff_perez+73 )=0.001600;
	*(*coeff_perez+74 )=1.147700;	*(*coeff_perez+75 )=0.106200;
	*(*coeff_perez+76 )=0.465900;	*(*coeff_perez+77 )=-0.329600;
	*(*coeff_perez+78 )=-0.087600;	*(*coeff_perez+79 )=-0.032900;
	*(*coeff_perez+80 )=-0.600000;	*(*coeff_perez+81 )=-0.356600;
	*(*coeff_perez+82 )=-2.500000;	*(*coeff_perez+83 )=2.325000;
	*(*coeff_perez+84 )=0.293700;	*(*coeff_perez+85 )=0.049600;
	*(*coeff_perez+86 )=-5.681200;	*(*coeff_perez+87 )=1.841500;
	*(*coeff_perez+88 )=21.000000;	*(*coeff_perez+89 )=-4.765600;
	*(*coeff_perez+90 )=-21.590599;	*(*coeff_perez+91 )=7.249200;
	*(*coeff_perez+92 )=-3.500000;	*(*coeff_perez+93 )=-0.155400;
	*(*coeff_perez+94 )=1.406200;	*(*coeff_perez+95 )=0.398800;
	*(*coeff_perez+96 )=0.003200;	*(*coeff_perez+97 )=0.076600;
	*(*coeff_perez+98 )=-0.065600;	*(*coeff_perez+99 )=-0.129400;
	*(*coeff_perez+100 )=-1.015600;	*(*coeff_perez+101 )=-0.367000;
	*(*coeff_perez+102 )=1.007800;	*(*coeff_perez+103 )=1.405100;
	*(*coeff_perez+104 )=0.287500;	*(*coeff_perez+105 )=-0.532800;
	*(*coeff_perez+106 )=-3.850000;	*(*coeff_perez+107 )=3.375000;
	*(*coeff_perez+108 )=14.000000;	*(*coeff_perez+109 )=-0.999900;
	*(*coeff_perez+110 )=-7.140600;	*(*coeff_perez+111 )=7.546900;
	*(*coeff_perez+112 )=-3.400000;	*(*coeff_perez+113 )=-0.107800;
	*(*coeff_perez+114 )=-1.075000;	*(*coeff_perez+115 )=1.570200;
	*(*coeff_perez+116 )=-0.067200;	*(*coeff_perez+117 )=0.401600;
	*(*coeff_perez+118 )=0.301700;	*(*coeff_perez+119 )=-0.484400;
	*(*coeff_perez+120 )=-1.000000;	*(*coeff_perez+121 )=0.021100;
	*(*coeff_perez+122 )=0.502500;	*(*coeff_perez+123 )=-0.511900;
	*(*coeff_perez+124 )=-0.300000;	*(*coeff_perez+125 )=0.192200;
	*(*coeff_perez+126 )=0.702300;	*(*coeff_perez+127 )=-1.631700;
	*(*coeff_perez+128 )=19.000000;	*(*coeff_perez+129 )=-5.000000;
	*(*coeff_perez+130 )=1.243800;	*(*coeff_perez+131 )=-1.909400;
	*(*coeff_perez+132 )=-4.000000;	*(*coeff_perez+133 )=0.025000;
	*(*coeff_perez+134 )=0.384400;	*(*coeff_perez+135 )=0.265600;
	*(*coeff_perez+136 )=1.046800;	*(*coeff_perez+137 )=-0.378800;
	*(*coeff_perez+138 )=-2.451700;	*(*coeff_perez+139 )=1.465600;
	*(*coeff_perez+140 )=-1.050000;	*(*coeff_perez+141 )=0.028900;
	*(*coeff_perez+142 )=0.426000;	*(*coeff_perez+143 )=0.359000;
	*(*coeff_perez+144 )=-0.325000;	*(*coeff_perez+145 )=0.115600;
	*(*coeff_perez+146 )=0.778100;	*(*coeff_perez+147 )=0.002500;
	*(*coeff_perez+148 )=31.062500;	*(*coeff_perez+149 )=-14.500000;
	*(*coeff_perez+150 )=-46.114799;*(*coeff_perez+151 )=55.375000;
	*(*coeff_perez+152 )=-7.231200;	*(*coeff_perez+153 )=0.405000;
	*(*coeff_perez+154 )=13.350000;	*(*coeff_perez+155 )=0.623400;
	*(*coeff_perez+156 )=1.500000;	*(*coeff_perez+157 )=-0.642600;
	*(*coeff_perez+158 )=1.856400;	*(*coeff_perez+159 )=0.563600;
	return 0;

}



/* sky luminance perez model */
double calc_rel_lum_perez(double dzeta,double gamma,double Z,
						  double epsilon,double Delta,float *coeff_perez)
{
	float x[5][4];
	int i,j,num_lin=0;
	double c_perez[5];

	if ( (epsilon <  skyclearinf) || (epsilon > skyclearsup) )
		{
			fprintf(stdout,"ds_illum: fatal error - Epsilon out of range in function calc_rel_lum_perez ! (%f) \n",epsilon);
			exit(1);
		}

	/* correction de modele de Perez solar energy ...*/
	if ( (epsilon > 1.065) && (epsilon < 2.8) )
		{
			if ( Delta < 0.2 ) Delta = 0.2;
		}

	if ( (epsilon >= 1.000) && (epsilon < 1.065) ) num_lin = 0;
	if ( (epsilon >= 1.065) && (epsilon < 1.230) ) num_lin = 1;
	if ( (epsilon >= 1.230) && (epsilon < 1.500) ) num_lin = 2;
	if ( (epsilon >= 1.500) && (epsilon < 1.950) ) num_lin = 3;
	if ( (epsilon >= 1.950) && (epsilon < 2.800) ) num_lin = 4;
	if ( (epsilon >= 2.800) && (epsilon < 4.500) ) num_lin = 5;
	if ( (epsilon >= 4.500) && (epsilon < 6.200) ) num_lin = 6;
	if ( (epsilon >= 6.200) && (epsilon < 14.00) ) num_lin = 7;

	for (i=0;i<5;i++)
		for (j=0;j<4;j++)
			{
				x[i][j] = *(coeff_perez + 20*num_lin + 4*i +j);
				/* printf("x %d %d vaut %f\n",i,j,x[i][j]); */
			}


	if (num_lin)
		{
			for (i=0;i<5;i++)
				c_perez[i] = x[i][0] + x[i][1]*Z + Delta * (x[i][2] + x[i][3]*Z);
		}
	else
		{
			c_perez[0] = x[0][0] + x[0][1]*Z + Delta * (x[0][2] + x[0][3]*Z);
			c_perez[1] = x[1][0] + x[1][1]*Z + Delta * (x[1][2] + x[1][3]*Z);
			c_perez[4] = x[4][0] + x[4][1]*Z + Delta * (x[4][2] + x[4][3]*Z);
			c_perez[2] = exp( pow(Delta*(x[2][0]+x[2][1]*Z),x[2][2])) - x[2][3];
			c_perez[3] = -exp( Delta*(x[3][0]+x[3][1]*Z) )+x[3][2]+Delta*x[3][3];
		}


	return (1 + c_perez[0]*exp(c_perez[1]/cos(dzeta)) ) *
	    (1 + c_perez[2]*exp(c_perez[3]*gamma) +
		 c_perez[4]*cos(gamma)*cos(gamma) );
}



/* coefficients for the sky luminance perez model */
void coeff_lum_perez(double Z, double epsilon, double Delta, float *coeff_perez)
{
	float x[5][4];
	int i,j,num_lin=0;

	if ( (epsilon <  skyclearinf) || (epsilon > skyclearsup) )
		{
			fprintf(stdout,"ds_illum: fatal error - Epsilon out of range in function calc_rel_lum_perez !\n");
			exit(1);
		}

	/* correction du modele de Perez solar energy ...*/
	if ( (epsilon > 1.065) && (epsilon < 2.8) )
		{
			if ( Delta < 0.2 ) Delta = 0.2;
		}

	if ( (epsilon >= 1.000) && (epsilon < 1.065) ) num_lin = 0;
	if ( (epsilon >= 1.065) && (epsilon < 1.230) ) num_lin = 1;
	if ( (epsilon >= 1.230) && (epsilon < 1.500) ) num_lin = 2;
	if ( (epsilon >= 1.500) && (epsilon < 1.950) ) num_lin = 3;
	if ( (epsilon >= 1.950) && (epsilon < 2.800) ) num_lin = 4;
	if ( (epsilon >= 2.800) && (epsilon < 4.500) ) num_lin = 5;
	if ( (epsilon >= 4.500) && (epsilon < 6.200) ) num_lin = 6;
	if ( (epsilon >= 6.200) && (epsilon < 14.00) ) num_lin = 7;

	for (i=0;i<5;i++)
		for (j=0;j<4;j++)
			{
				x[i][j] = *(coeff_perez + 20*num_lin + 4*i +j);
				/* printf("x %d %d vaut %f\n",i,j,x[i][j]); */
			}


	if (num_lin)
		{
			for (i=0;i<5;i++)
				c_perez[i] = x[i][0] + x[i][1]*Z + Delta * (x[i][2] + x[i][3]*Z);

		}
	else
		{
			c_perez[0] = x[0][0] + x[0][1]*Z + Delta * (x[0][2] + x[0][3]*Z);
			c_perez[1] = x[1][0] + x[1][1]*Z + Delta * (x[1][2] + x[1][3]*Z);
			c_perez[4] = x[4][0] + x[4][1]*Z + Delta * (x[4][2] + x[4][3]*Z);
			c_perez[2] = exp( pow(Delta*(x[2][0]+x[2][1]*Z),x[2][2])) - x[2][3];
			c_perez[3] = -exp( Delta*(x[3][0]+x[3][1]*Z) )+x[3][2]+Delta*x[3][3];
		}

	return;
}


/* degrees into radians */
double radians(double degres)
{
	return degres*M_PI/180.0;
}

/* radian into degrees */
double degres(double radians)
{
	return radians/M_PI*180.0;
}

/* calculation of the angles dzeta and gamma */
void theta_phi_to_dzeta_gamma(double theta,double phi,double *dzeta,double *gamma, double Z)
{
	*dzeta = theta; /* dzeta = phi */
	if ( (cos(Z)*cos(theta)+sin(Z)*sin(theta)*cos(phi)) > 1 && (cos(Z)*cos(theta)+sin(Z)*sin(theta)*cos(phi) < 1.1 ) )
		*gamma = 0;
	else if ( (cos(Z)*cos(theta)+sin(Z)*sin(theta)*cos(phi)) > 1.1 )
		{
			printf("error in calculation of gamma (angle between point and sun");
			exit(3);
		}
	else
		*gamma = acos(cos(Z)*cos(theta)+sin(Z)*sin(theta)*cos(phi));
}


float *theta_ordered()
{
	float *ptr;

	if ( (ptr = malloc(145*sizeof(float))) == NULL )
		{
			fprintf(stdout,"ds_illum: fatal error - Out of memory in function theta_ordered\n");
			exit(1);
		}

	*(ptr+0)=84;	*(ptr+1)=84;	*(ptr+2)=84;	*(ptr+3)=84;	*(ptr+4)=84;
	*(ptr+5)=84;	*(ptr+6)=84;	*(ptr+7)=84;	*(ptr+8)=84;	*(ptr+9)=84;
	*(ptr+10)=84;	*(ptr+11)=84;	*(ptr+12)=84;	*(ptr+13)=84;	*(ptr+14)=84;
	*(ptr+15)=84;	*(ptr+16)=84;	*(ptr+17)=84;	*(ptr+18)=84;	*(ptr+19)=84;
	*(ptr+20)=84;	*(ptr+21)=84;	*(ptr+22)=84;	*(ptr+23)=84;	*(ptr+24)=84;
	*(ptr+25)=84;	*(ptr+26)=84;	*(ptr+27)=84;	*(ptr+28)=84;	*(ptr+29)=84;
	*(ptr+30)=72;	*(ptr+31)=72;	*(ptr+32)=72;	*(ptr+33)=72;	*(ptr+34)=72;
	*(ptr+35)=72;	*(ptr+36)=72;	*(ptr+37)=72;	*(ptr+38)=72;	*(ptr+39)=72;
	*(ptr+40)=72;	*(ptr+41)=72;	*(ptr+42)=72;	*(ptr+43)=72;	*(ptr+44)=72;
	*(ptr+45)=72;	*(ptr+46)=72;	*(ptr+47)=72;	*(ptr+48)=72;	*(ptr+49)=72;
	*(ptr+50)=72;	*(ptr+51)=72;	*(ptr+52)=72;	*(ptr+53)=72;	*(ptr+54)=72;
	*(ptr+55)=72;	*(ptr+56)=72;	*(ptr+57)=72;	*(ptr+58)=72;	*(ptr+59)=72;
	*(ptr+60)=60;	*(ptr+61)=60;	*(ptr+62)=60;	*(ptr+63)=60;	*(ptr+64)=60;
	*(ptr+65)=60;	*(ptr+66)=60;	*(ptr+67)=60;	*(ptr+68)=60;	*(ptr+69)=60;
	*(ptr+70)=60;	*(ptr+71)=60;	*(ptr+72)=60;	*(ptr+73)=60;	*(ptr+74)=60;
	*(ptr+75)=60;	*(ptr+76)=60;	*(ptr+77)=60;	*(ptr+78)=60;	*(ptr+79)=60;
	*(ptr+80)=60;	*(ptr+81)=60;	*(ptr+82)=60;	*(ptr+83)=60;	*(ptr+84)=48;
	*(ptr+85)=48;	*(ptr+86)=48;	*(ptr+87)=48;	*(ptr+88)=48;	*(ptr+89)=48;
	*(ptr+90)=48;	*(ptr+91)=48;	*(ptr+92)=48;	*(ptr+93)=48;	*(ptr+94)=48;
	*(ptr+95)=48;	*(ptr+96)=48;	*(ptr+97)=48;	*(ptr+98)=48;	*(ptr+99)=48;
	*(ptr+100)=48;	*(ptr+101)=48;	*(ptr+102)=48;	*(ptr+103)=48;	*(ptr+104)=48;
	*(ptr+105)=48;	*(ptr+106)=48;	*(ptr+107)=48;	*(ptr+108)=36;	*(ptr+109)=36;
	*(ptr+110)=36;	*(ptr+111)=36;	*(ptr+112)=36;	*(ptr+113)=36;	*(ptr+114)=36;
	*(ptr+115)=36;	*(ptr+116)=36;	*(ptr+117)=36;	*(ptr+118)=36;	*(ptr+119)=36;
	*(ptr+120)=36;	*(ptr+121)=36;	*(ptr+122)=36;	*(ptr+123)=36;	*(ptr+124)=36;
	*(ptr+125)=36;	*(ptr+126)=24;	*(ptr+127)=24;	*(ptr+128)=24;	*(ptr+129)=24;
	*(ptr+130)=24;	*(ptr+131)=24;	*(ptr+132)=24;	*(ptr+133)=24;	*(ptr+134)=24;
	*(ptr+135)=24;	*(ptr+136)=24;	*(ptr+137)=24;	*(ptr+138)=12;	*(ptr+139)=12;
	*(ptr+140)=12;	*(ptr+141)=12;	*(ptr+142)=12;	*(ptr+143)=12;	*(ptr+144)=0;
	return ptr;
}


float *phi_ordered()
{
	float *ptr;

	if ( (ptr = malloc(145*sizeof(float))) == NULL ) {
		fprintf(stdout,"ds_illum: fatal error - Out of memory in function phi_ordered");
		exit(1);
	}
	*(ptr+0)=0;		*(ptr+1)=12;		*(ptr+2)=24;		*(ptr+3)=36;
	*(ptr+4)=48;	*(ptr+5)=60;		*(ptr+6)=72;		*(ptr+7)=84;
	*(ptr+8)=96;	*(ptr+9)=108;		*(ptr+10)=120;		*(ptr+11)=132;
	*(ptr+12)=144;	*(ptr+13)=156;		*(ptr+14)=168;		*(ptr+15)=180;
	*(ptr+16)=192;	*(ptr+17)=204;		*(ptr+18)=216;		*(ptr+19)=228;
	*(ptr+20)=240;	*(ptr+21)=252;		*(ptr+22)=264;		*(ptr+23)=276;
	*(ptr+24)=288;	*(ptr+25)=300;		*(ptr+26)=312;		*(ptr+27)=324;
	*(ptr+28)=336;	*(ptr+29)=348;		*(ptr+30)=0;		*(ptr+31)=12;
	*(ptr+32)=24;	*(ptr+33)=36;		*(ptr+34)=48;		*(ptr+35)=60;
	*(ptr+36)=72;	*(ptr+37)=84;		*(ptr+38)=96;		*(ptr+39)=108;
	*(ptr+40)=120;	*(ptr+41)=132;		*(ptr+42)=144;		*(ptr+43)=156;
	*(ptr+44)=168;	*(ptr+45)=180;		*(ptr+46)=192;		*(ptr+47)=204;
	*(ptr+48)=216;	*(ptr+49)=228;		*(ptr+50)=240;		*(ptr+51)=252;
	*(ptr+52)=264;	*(ptr+53)=276;		*(ptr+54)=288;		*(ptr+55)=300;
	*(ptr+56)=312;	*(ptr+57)=324;		*(ptr+58)=336;		*(ptr+59)=348;
	*(ptr+60)=0;	*(ptr+61)=15;		*(ptr+62)=30;		*(ptr+63)=45;
	*(ptr+64)=60;	*(ptr+65)=75;		*(ptr+66)=90;		*(ptr+67)=105;
	*(ptr+68)=120;	*(ptr+69)=135;		*(ptr+70)=150;		*(ptr+71)=165;
	*(ptr+72)=180;	*(ptr+73)=195;		*(ptr+74)=210;		*(ptr+75)=225;
	*(ptr+76)=240;	*(ptr+77)=255;		*(ptr+78)=270;		*(ptr+79)=285;
	*(ptr+80)=300;	*(ptr+81)=315;		*(ptr+82)=330;		*(ptr+83)=345;
	*(ptr+84)=0;	*(ptr+85)=15;		*(ptr+86)=30;		*(ptr+87)=45;
	*(ptr+88)=60;	*(ptr+89)=75;		*(ptr+90)=90;		*(ptr+91)=105;
	*(ptr+92)=120;	*(ptr+93)=135;		*(ptr+94)=150;		*(ptr+95)=165;
	*(ptr+96)=180;	*(ptr+97)=195;		*(ptr+98)=210;		*(ptr+99)=225;
	*(ptr+100)=240;	*(ptr+101)=255;		*(ptr+102)=270;		*(ptr+103)=285;
	*(ptr+104)=300;	*(ptr+105)=315;		*(ptr+106)=330;		*(ptr+107)=345;
	*(ptr+108)=0;	*(ptr+109)=20;		*(ptr+110)=40;		*(ptr+111)=60;
	*(ptr+112)=80;	*(ptr+113)=100;		*(ptr+114)=120;		*(ptr+115)=140;
	*(ptr+116)=160;	*(ptr+117)=180;		*(ptr+118)=200;		*(ptr+119)=220;
	*(ptr+120)=240;	*(ptr+121)=260;		*(ptr+122)=280;		*(ptr+123)=300;
	*(ptr+124)=320;	*(ptr+125)=340;		*(ptr+126)=0;		*(ptr+127)=30;
	*(ptr+128)=60;	*(ptr+129)=90;		*(ptr+130)=120;		*(ptr+131)=150;
	*(ptr+132)=180;	*(ptr+133)=210;		*(ptr+134)=240;		*(ptr+135)=270;
	*(ptr+136)=300;	*(ptr+137)=330;		*(ptr+138)=0;		*(ptr+139)=60;
	*(ptr+140)=120;	*(ptr+141)=180;		*(ptr+142)=240;		*(ptr+143)=300;
	*(ptr+144)=0;

	return ptr;
}


/********************************************************************************/
/*	Fonction: integ_lv							*/
/*										*/
/*	In: float *lv,*theta							*/
/*	    int sun_pos								*/
/*										*/
/*	Out: double								*/
/*										*/
/*	Update: 29/08/93							*/
/*										*/
/*	But: calcul l'integrale de luminance relative sans la dir. du soleil	*/
/*										*/
/********************************************************************************/
double integ_lv(float *lv,float *theta)
{
	int i;
	double buffer=0.0;

	for (i=0;i<145;i++)
		buffer += (*(lv+i))*cos(radians(*(theta+i)));

	return buffer*2*M_PI/144;
}


/* enter day number(double), return E0 = square(R0/R):  eccentricity correction factor  */
double get_eccentricity()
{
	double day_angle;
	double E0;

	day_angle  = 2*M_PI*(daynumber - 1)/365;
	E0         = 1.00011+0.034221*cos(day_angle)+0.00128*sin(day_angle)+
	    0.000719*cos(2*day_angle)+0.000077*sin(2*day_angle);

	return (E0);
}

/* enter sunzenith angle (degrees) return relative air mass (double) */
double 	air_mass()
{
	double	m;
	m = 1/( cos(sunzenith*M_PI/180)+0.15*exp( log(93.885-sunzenith)*(-1.253) ) );
	return(m);
}


double get_angle_sun_direction(double sun_zenith, double sun_azimut, double direction_zenith, double direction_azimut)
{
	double angle;
	if (sun_zenith == 0)
        puts("WARNING: zenith_angle = 0 in function get_angle_sun_vert_plan");

	angle = acos( cos(sun_zenith*M_PI/180)*cos(direction_zenith*M_PI/180) + sin(sun_zenith*M_PI/180)*sin(direction_zenith*M_PI/180)*cos((sun_azimut-direction_azimut)*M_PI/180) );
	angle = angle*180/M_PI;
	return(angle);
}