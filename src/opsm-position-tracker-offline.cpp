//============================================================================
// Name        : obserbation-probability-position-tracker.cpp
// Author      : tyamada
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================

#include <stdio.h>
#include <time.h>

#include <ssmtype/spur-odometry.h>
#include <ssm.hpp>
#include <ssm-log.hpp>
#include "ssm-laser.hpp"

#include "opsm-position-tracker-offline-opt.hpp"
#include "opsm-position-tracker-offline-cui.hpp"

#include "gnd-coord-tree.hpp"
#include "gnd-matrix-coordinate.hpp"
#include "gnd-matrix-base.hpp"

#include "gnd-opsm.hpp"
#include "gnd-odometry-correction.hpp"
#include "gnd-gridmap.hpp"
#include "gnd-shutoff.hpp"
#include "gnd-timer.hpp"
#include "gnd-bmp.hpp"


static const double ShowCycle = gnd_sec2time(1.0);

int main(int argc, char* argv[]) {
	gnd::opsm::optimize_basic	*optimizer = 0;		// optimizer class
	void 						*optim_ini = 0;		// optimization starting value

	gnd::opsm::cmap_t			cnt_smmap;			// probabilistic scan matching counting map
	gnd::opsm::map_t				smmap;				// probabilistic scan matching map

	SSMLogBase					ssmlog_sokuikiraw;	// sokuiki raw streaming data
	void*						sokuikiraw_buf;
	ssm::ScanPoint2D			sokuikiraw;
	ssm::ScanPoint2DProperty	sokuikiraw_prop;
	SSMLog<Spur_Odometry>		ssmlog_odometry;	// odometry position streaming data


	gnd::matrix::coord_tree coordtree;				// coordinate tree
	int coordid_gl = -1,							// global coordinate node id
			coordid_rbt = -1,						// robot coordinate node id
			coordid_sns = -1,						// sensor coordinate node id
			coordid_odm = -1;						// odometry coordinate node id

	gnd::cui_reader gcui;							// cui manager

	gnd::odometry::cmap cmap;

	opsm_pt::proc_configuration pconf;			// configuration parameter
	opsm_pt::options popt(&pconf);				// process option analyze class

	FILE *tlog_fp = 0;
	FILE *llog_fp = 0;
	FILE *t4re_fp = 0;

	{
		gnd::opsm::debug_set_log_level(0);
		gnd::opsm::debug_set_fstream("debug.log");
	}


	{ // ---> initialization
		int ret;								// function return value
		uint32_t phase = 1;						// initialize phase

		// ---> read process options
		if( (ret = popt.get_option(argc, argv)) != 0 ) {
			return ret;
		} // <--- read process options



		{ // ---> coordinate-tree set robot coordinate
			gnd::coord_matrix cc; // coordinate relation matrix

			// set global coordinate
			gnd::matrix::set_unit(&cc);
			coordid_gl = coordtree.add("global", "root", &cc);

			// set robot coordinate
			gnd::matrix::set_unit(&cc);
			coordid_rbt = coordtree.add("robot", "global", &cc);

			// set odometry coordinate
			gnd::matrix::set_unit(&cc);
			coordid_odm = coordtree.add("odometry", "root", &cc);	// local dead-reckoning

			// set scan matching coordinate
//			gnd::matrix::set_unit(&cc);
//			coordid_sm = coordtree.add("scan-matching", "global", &cc);	// this coordinate's origin is scan matching result position

			// set scan matching coordinate
			gnd::matrix::set_unit(&cc);
			coordid_sns = coordtree.add("sensor", "robot", &cc);

			// set scan matching coordinate
//			gnd::matrix::set_unit(&cc);
//			coordid_sns_sm = coordtree.add("sensor", "scan-matching", &cc);

		} // <--- coordinate-tree set robot coordinate



		{ // ---> allocate SIGINT to shut-off
			::proc_shutoff_clear();
			::proc_shutoff_alloc_signal(SIGINT);
		} // <--- allocate SIGINT to shut-off



		{ // ---> show initialize task
			::fprintf(stderr, "==========Initialize==========\n");
			if( *pconf.cmap.value ) {
				::fprintf(stderr, " %d. load correction map from \"\x1b[4m%s\x1b[0m\"\n", phase++, pconf.cmap.value);
			}
			::fprintf(stderr, " %d. create optimizer class \"\x1b[4m%s\x1b[0m\"\n", phase++, pconf.optimizer.value);
			if( pconf.map_update.value && *pconf.init_opsm_map.value ) {
				::fprintf(stderr, " %d. Map Data Load\n", phase++);
				::fprintf(stderr, " %d. Build %sMap\n", phase++, pconf.ndt.value ? "NDT " : "");
			}
			::fprintf(stderr, " %d. Open ssm-log \"\x1b[4m%s\x1b[0m\"\n", phase++, pconf.ls_logname.value);
			::fprintf(stderr, " %d. Open ssm-log \"\x1b[4m%s\x1b[0m\"\n", phase++, pconf.odm_logname.value);
			::fprintf(stderr, " %d. Initialize viewer\n", phase++);
			::fprintf(stderr, "\n\n");
		} // <--- show initialize task




		// ---> load odometry correction map
		if( !::is_proc_shutoff() && *pconf.cmap.value ) {
			::fprintf(stderr, "\n");
			::fprintf(stderr, " => load odometry error correction map \"\x1b[4m%s\x1b[0m\"\n", pconf.cmap.value);
			if( cmap.fread(pconf.cmap.value) < 0 ) {
				::proc_shutoff();
				::fprintf(stderr, "  ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: fail to load odometry error correction map \"\x1b[4m%s\x1b[0m\"\n", pconf.cmap.value);
			}
			else {
				::fprintf(stderr, "  ... \x1b[1mOK\x1b[0m: load odometry error correction map \"\x1b[4m%s\x1b[0m\"\n", pconf.cmap.value);
			}
		} // <--- load odometry correction map




		// ---> set optimizer
		if( !::is_proc_shutoff() ) {
			::fprintf(stderr, "\n");
			::fprintf(stderr, " => create optimizer class \"\x1b[4m%s\x1b[0m\"\n", pconf.optimizer.value);
			// Newton Method
			if( !::strcmp(pconf.optimizer.value, opsm_pt::OptNewton) ){
				optimizer = new gnd::opsm::newton;
				optimizer->initial_parameter_create(&optim_ini);
				optimizer->set_converge_threshold(pconf.converge_dist.value, pconf.converge_orient.value );
				::fprintf(stderr, " ... newton's method \x1b[1mOK\x1b[0m\n");
			}
			// Monte Calro Method
			else if( !::strcmp(pconf.optimizer.value, opsm_pt::OptMCL)){
				gnd::opsm::mcl::initial_parameter *p;
				optimizer = new gnd::opsm::mcl;
				optimizer->initial_parameter_create(&optim_ini);
				p = static_cast<gnd::opsm::mcl::initial_parameter*>(optim_ini);
				p->n = 1000;
				p->alpha = 0.05;
				optim_ini = static_cast<void*>(p);
				optimizer->set_converge_threshold(pconf.converge_dist.value, pconf.converge_orient.value );
				::fprintf(stderr, "  ... monte calro method \x1b[1mOK\x1b[0m\n");
			}
			// Quasi Monte Calro Method
			else if( !::strcmp(pconf.optimizer.value, opsm_pt::OptQMC)){
				gnd::opsm::qmc::initial_parameter *p;
				optimizer = new gnd::opsm::qmc;
				optimizer->initial_parameter_create(&optim_ini);
				p = static_cast<gnd::opsm::qmc::initial_parameter*>(optim_ini);
				p->n = 2;
				optim_ini = static_cast<void*>(p);
				optimizer->set_converge_threshold(pconf.converge_dist.value, pconf.converge_orient.value );
				::fprintf(stderr, "  ... quasi monte calro method \x1b[1mOK\x1b[0m\n");
			}
			else if( !::strcmp(pconf.optimizer.value, opsm_pt::OptQMC2Newton)){
				gnd::opsm::hybrid_q2n::initial_parameter *p;
				optimizer = new gnd::opsm::hybrid_q2n;
				optimizer->initial_parameter_create(&optim_ini);
				p = static_cast<gnd::opsm::hybrid_q2n::initial_parameter*>(optim_ini);
				p->n = 2;
				optim_ini = static_cast<void*>(p);
				optimizer->set_converge_threshold(pconf.converge_dist.value, pconf.converge_orient.value );
				::fprintf(stderr, "  ... quasi monte calro and newton hybrid \x1b[1mOK\x1b[0m\n");
			}
			else {
				::proc_shutoff();
				::fprintf(stderr, "  ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: invalid optimizer type\n");
			}

		} // ---> set optimizer




		// ---> build map
		if( !::is_proc_shutoff() && pconf.map_update.value && *pconf.init_opsm_map.value ){
			::fprintf(stderr, "\n");
			::fprintf(stderr, " => load scan matching map from \"\x1b[4m%s\x1b[0m\"\n", pconf.init_opsm_map.value);
			if( gnd::opsm::read_counting_map(&cnt_smmap, pconf.init_opsm_map.value) < 0){
				::proc_shutoff();
				::fprintf(stderr, "  ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: fail to load scan matching map \"\x1b[4m%s\x1b[0m\"\n", pconf.init_opsm_map.value);
			}
			else if( !pconf.ndt.value){
				if( gnd::opsm::build_map(&smmap, &cnt_smmap, gnd_mm2dist(1)) < 0) {
					::proc_shutoff();
					::fprintf(stderr, "  ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: fail to build scan matching map \"\x1b[4m%s\x1b[0m\"\n", pconf.init_opsm_map.value);
				}
				else {
					::fprintf(stderr, "  ... \x1b[1mOK\x1b[0m: load scan matching map \"\x1b[4m%s\x1b[0m\"\n", pconf.init_opsm_map.value);
				}
			}
			else {
				if(gnd::opsm::build_ndt_map(&smmap, &cnt_smmap, gnd_mm2dist(1)) < 0){
					::proc_shutoff();
					::fprintf(stderr, "  ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: fail to build scan matching map \"\x1b[4m%s\x1b[0m\"\n", pconf.init_opsm_map.value);
				}
				else {
					::fprintf(stderr, "  ... \x1b[1mOK\x1b[0m: load scan matching map \"\x1b[4m%s\x1b[0m\"\n", pconf.init_opsm_map.value);
				}
			}
		} // <--- build map


		// set map
		if(!::is_proc_shutoff() )	optimizer->set_map(&smmap);



		// ---> open ssm odometry
		if( !::is_proc_shutoff() ){
			::fprintf(stderr, "\n");
			::fprintf(stderr, " => load ssm-data \"\x1b[4m%s\x1b[0m\"\n", pconf.odm_logname.value);
			if( !(*pconf.odm_logname.value) ){
				// shut off
				::proc_shutoff();
				::fprintf(stderr, " ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: missing log file operand\n" );
			}
			else {
				::fprintf(stderr, "    File \"\x1b[4m%s\x1b[0m\"\n", pconf.odm_logname.value);

				if( !ssmlog_odometry.open( pconf.odm_logname.value ) ){
					::proc_shutoff();
					::fprintf(stderr, "  [\x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m]: fail to load ssm-log \"\x1b[4m%s\x1b[0m\"\n", pconf.odm_logname.value);
				}
				else {
					::fprintf(stderr, "   ...\x1b[1mOK\x1b[0m: Open ssm-data \"\x1b[4m%s\x1b[0m\"\n", pconf.odm_logname.value);
					{ // ---> set coordinate
						gnd::matrix::fixed<4,4> pos_cc;

						// odometry coordinate
						gnd::matrix::coordinate_converter(&pos_cc,
								0, 0, 0,
								::cos(0), ::sin(0), 0,
								 0, 0, 1);

						coordtree.set_coordinate(coordid_odm, &pos_cc);
					} // ---> set coordinate
				}
			}
		} // <--- open ssm odometry



		// ---> open ssm sokuiki raw data
		if(!::is_proc_shutoff()){
			::fprintf(stderr, "\n");
			::fprintf(stderr, " => load ssm-data \"\x1b[4m%s\x1b[0m\"\n", pconf.ls_logname.value);

			if( ! *pconf.ls_logname.value ){
				::proc_shutoff();
				::fprintf(stderr, " ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: missing log file operand\n" );
				::fprintf(stderr, "     please show help, ./%s -S <sokuiki.log> -O <odometry.log>\n", opsm_pt::proc_name );
			}
			else {
				::fprintf(stderr, "    File \"\x1b[4m%s\x1b[0m\"\n", pconf.odm_logname.value);

				if( !ssmlog_sokuikiraw.open( pconf.ls_logname.value ) ){
					::proc_shutoff();
					::fprintf(stderr, " ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: fail to load ssm-log \"\x1b[4m%s\x1b[0m\"\n", pconf.ls_logname.value);
				}
				// get property
				else {
					// read property
					ssmlog_sokuikiraw.setBuffer(0, 0, &sokuikiraw_prop, sizeof(sokuikiraw_prop) );
					ssmlog_sokuikiraw.readProperty();

					// allocate memory
					sokuikiraw.alloc( sokuikiraw_prop.numPoints );
					sokuikiraw_buf = (void*) malloc( sokuikiraw._ssmSize() );

					ssmlog_sokuikiraw.setBuffer(sokuikiraw_buf, sokuikiraw._ssmSize(), &sokuikiraw_prop, sizeof(sokuikiraw_prop) );
					ssmlog_sokuikiraw.readNext();
                    ssm::ScanPoint2D::_ssmRead( ssmlog_sokuikiraw.data(), &sokuikiraw, 0);

					{ // ---> coordinate-tree set sensor coordinate
						coordid_sns = coordtree.add("sensor", "robot", &sokuikiraw_prop.coordm);
					} // <--- coordinate-tree set robot coordinate
					::fprintf(stderr, " ... \x1b[1mOK\x1b[0m: Open ssm-data \"\x1b[4m%s\x1b[0m\n", pconf.ls_logname.value);
				}
			}
		} // <--- open ssm sokuiki raw data


		// ---> make output directory
		if( !::is_proc_shutoff() && *pconf.output_dir.value ) {
			::fprintf(stderr, "\n");
			::fprintf(stderr, " => make output directory \"\x1b[4m%s\x1b[0m\"\n", pconf.output_dir.value);

			errno = 0;
			if( mkdir(pconf.output_dir.value, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH ) < 0) {
				if( errno != EEXIST ) {
					::proc_shutoff();
					::fprintf(stderr, " ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: fail to make output directory \"\x1b[4m%s\x1b[0m\"\n", pconf.output_dir.value);
				}
				else {
					struct stat st;
					::stat(pconf.output_dir.value, &st);

					if( S_ISDIR(st.st_mode)) {
						::fprintf(stderr, " ...\x1b[1mOK\x1b[0m: output directory \"\x1b[4m%s\x1b[0m\" is already exist\n", pconf.output_dir.value);
					}
					else {
						::proc_shutoff();
						::fprintf(stderr, " ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: \"\x1b[4m%s\x1b[0m\" is already exist and it is not directory\n", pconf.output_dir.value);
					}
				}
			}
			else {
				::fprintf(stderr, " ...\x1b[1mOK\x1b[0m: make output directory \"\x1b[4m%s\x1b[0m\"\n", pconf.output_dir.value);
			}
		} // <--- make output directory


		if ( !::is_proc_shutoff() && *pconf.trajectory_log.value) {
			char fname[512];
			::fprintf(stderr, "\n");
			::fprintf(stderr, " => open trajectory log file\n");

			if( ::snprintf(fname, sizeof(fname), "%s/%s", *pconf.output_dir.value ? pconf.output_dir.value : "./", pconf.trajectory_log.value) == sizeof(fname) ){
				::proc_shutoff();
				::fprintf(stderr, " ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: file path is too long\n");
			}
			else if( !(tlog_fp = fopen( fname, "w" )) ) {
				::proc_shutoff();
				::fprintf(stderr, " ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: fail to open \"\x1b[4m%s\x1b[0m\"\n", fname);
			}
			else {
				::fprintf(tlog_fp, "# 1.[time, s] 2.[x] 3.[y] 4.[theta] 5.[v] 6.[w]. 7.[adjust-x] 8.[adjust-y] 9.[adjust-theta] 10.[adjust-v] 11.[adjust-w].\n");
				::fprintf(stderr, "  ... \x1b[1mOK\x1b[0m\n");
			}
		}

		if ( !::is_proc_shutoff() && *pconf.laserpoint_log.value) {
			char fname[512];
			::fprintf(stderr, "\n");
			::fprintf(stderr, " => open laser point log file\n");

			if( ::snprintf(fname, sizeof(fname), "%s/%s", *pconf.output_dir.value ? pconf.output_dir.value : "./", pconf.laserpoint_log.value) == sizeof(fname) ){
				::proc_shutoff();
				::fprintf(stderr, " ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: file path is too long\n");
			}
			else if( !(llog_fp = fopen( fname, "w" )) ) {
				::proc_shutoff();
				::fprintf(stderr, " ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: fail to open \"\x1b[4m%s\x1b[0m\"\n", fname);
			}
			else {
				::fprintf(stderr, "  ... \x1b[1mOK\x1b[0m\n");
			}
		}

		if ( !::is_proc_shutoff() && *pconf.trajectory4route.value) {
			char fname[512];
			::fprintf(stderr, "\n");
			::fprintf(stderr, " => open trajectory file for route edit\n");

			if( ::snprintf(fname, sizeof(fname), "%s/%s", *pconf.output_dir.value ? pconf.output_dir.value : "./", pconf.trajectory4route.value) == sizeof(fname) ){
				::proc_shutoff();
				::fprintf(stderr, " ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: file path is too long\n");
			}
			else if( !(t4re_fp = fopen( fname, "w" )) ) {
				::proc_shutoff();
				::fprintf(stderr, " ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: fail to open \"\x1b[4m%s\x1b[0m\"\n", fname);
			}
			else {
				::fprintf(stderr, "  ... \x1b[1mOK\x1b[0m\n");
			}
		}


		if ( !::is_proc_shutoff() && !cmap.is_allocate()) {
			// create map
			gnd::odometry::correction::create(&cmap, pconf.pos_gridsizex.value, pconf.pos_gridsizey.value, pconf.ang_rsl.value);
		}


		// set cui command
		gcui.set_command(opsm_pt::cui_cmd, sizeof(opsm_pt::cui_cmd) / sizeof(opsm_pt::cui_cmd[0]));


		// fin of initialization
		::fprintf(stderr, "\n\n");
	} // <--- initialization












	// ---> operation
	if(!::is_proc_shutoff() ){
		int ret = 0;									// function return value
		int cnt = 0;

		Spur_Odometry odo_prevloop = ssmlog_odometry.data(); // previous odometry position
		Spur_Odometry move_est;							// estimation of movement quantity

		double culling_sqdist							// data decimation threshold
		= gnd_square( pconf.culling.value );
		double lkl = 0;									// likelihood
		Spur_Odometry pos_opt = ssmlog_odometry.data(); // optimized position
		int cnt_opt = 0;								// optimization loop counter
		int cnt_correct = 0;

		double prev_time = ssmlog_odometry.time();			// previous odometry estimate time
		gnd::matrix::fixed<3,1> move_opt;				// position estimation movement by optimization
		double move_dist_pause = 0;							// change value on distance between previous and current frame
		double move_orient_puase = 0;						// change value on orientation between previous and current frame
		double move_dist_map = 0;							// change value on distance between previous and current frame
		double move_orient_map = 0;						// change value on orientation between previous and current frame

		double init_time = ssmlog_odometry.time();
		Spur_Odometry pos = ssmlog_odometry.data();
		Spur_Odometry pos_premap = ssmlog_odometry.data();	// odometry position streaming data
		ssmTimeT time_premap = 0;						// previous map update time

		gnd::matrix::fixed<4,4> coordm_sns2rbt;			// coordinate convert matrix from sensor to robot
		gnd::matrix::fixed<4,4> coordm_sns2gl;			// coordinate convert matrix from sensor to global

		double cuito = 0;								// blocking time out for cui input

		gnd::timer::interval_timer timer_show;			// time operation timer
		int nline_show;

		bool mapupdate = false;
		int cnt_mapupdate = 0;

		int cnt_fail = 0;

		// get coordinate convert matrix
		coordtree.get_convert_matrix(coordid_sns, coordid_rbt, &coordm_sns2rbt);

		{ // ---> set zero
			move_est.x = 0;
			move_est.y = 0;
			move_est.theta = 0;
			move_est.v = 0;
			move_est.w = 0;
		} // <--- set zero



		// ---> memory allocate counting map
		if( !cnt_smmap.plane[0].is_allocate() ){
			gnd::opsm::init_counting_map(&cnt_smmap, 1.0, 10);
		} // <--- memory allocate counting map


		if( !(*pconf.init_opsm_map.value) ){ // ---> map initialization
			int cnt_ls = 0;

			::fprintf(stderr, "-------------------- map initialize  --------------------\n");
			// ---> map initialization loop
			while( !::is_proc_shutoff() && cnt_ls < pconf.ini_map_cnt.value ) {

				// ---> read ssm-sokuikiraw-data
				if(ssmlog_sokuikiraw.readNext()) {
					{ // ---> 1. compute position estimation from odometry
						// get position on odometry position (on odometry cooordinate)
						if( !ssmlog_odometry.readTime( ssmlog_sokuikiraw.time() ) ) continue;

	                    ssm::ScanPoint2D::_ssmRead( ssmlog_sokuikiraw.data(), &sokuikiraw, 0);

						{ // ---> compute the movement estimation
							gnd::vector::fixed_column<4> odov_cprev;		// current odometry position vector on previous odometry coordinate

							{ // ---> compute current odometry position on previous odometry coordinate
								gnd::matrix::fixed<4,4> coordm_r2podo;		// coordinate matrix of previous odometry position
								gnd::vector::fixed_column<4> ws4x1;

								// get previous odometry coordinate matrix
								coordtree.get_convert_matrix(0, coordid_odm, &coordm_r2podo);

								// multiply previous odometry coordinate matrix with current position vector
								ws4x1[0] = ssmlog_odometry.data().x;
								ws4x1[1] = ssmlog_odometry.data().y;
								ws4x1[2] = 0;
								ws4x1[3] = 1;
								gnd::matrix::prod(&coordm_r2podo, &ws4x1, &odov_cprev);
							} // <--- compute current odometry position on previous odometry coordinate

							// get movement estimation by odometry
							move_est.x = odov_cprev[0];
							move_est.y = odov_cprev[1];
							move_est.theta = ssmlog_odometry.data().theta - odo_prevloop.theta;
						} // <--- compute the movement estimation


						{ // ---> add movement estimation
							gnd::vector::fixed_column<4> pos_odmest;

							{ // ---> compute position estimation by odometry on global coordinate
								gnd::matrix::fixed<4,4> coordm_rbt2gl;		// coordinate convert matrix from robot to global
								gnd::vector::fixed_column<4> ws4x1;

								// set search position on sensor-coordinate
								coordtree.get_convert_matrix(coordid_rbt, coordid_gl, &coordm_rbt2gl);

								ws4x1[0] = move_est.x;
								ws4x1[1] = move_est.y;
								ws4x1[2] = 0;
								ws4x1[3] = 1;

								gnd::matrix::prod(&coordm_rbt2gl, &ws4x1, &pos_odmest);
							} // <--- compute position estimation by odometry on global coordinate

							// set position
							pos.x = pos_odmest[0];
							pos.y = pos_odmest[1];
							pos.theta += move_est.theta;
						} // <--- add movement estimation
					}  // <--- 1. compute position estimation from odometry


					{ // ---> 2. update robot position coordinate and odometory position coordinate
						gnd::matrix::fixed<4,4> coordm;

						// odometry coordinate
						gnd::matrix::coordinate_converter(&coordm,
								ssmlog_odometry.data().x, ssmlog_odometry.data().y, 0,
								::cos(ssmlog_odometry.data().theta), ::sin(ssmlog_odometry.data().theta), 0,
								 0, 0, 1);
						coordtree.set_coordinate(coordid_odm, &coordm);

						// robot position coordinate
						gnd::matrix::coordinate_converter(&coordm,
								pos.x, pos.y, 0,
								::cos( pos.theta ), ::sin( pos.theta ), 0,
								 0, 0, 1);
						coordtree.set_coordinate(coordid_rbt, &coordm);

						// get coordinate convert matrix
						coordtree.get_convert_matrix(coordid_sns, coordid_gl, &coordm_sns2gl);
					} // ---> 2. update robot position coordinate and odometory position coordinate

					gnd::matrix::set_zero(&move_opt);
					{ // ---> 3. entry laser scanner reading
						gnd::vector::fixed_column<3> delta;
						gnd::vector::fixed_column<2> reflect_prevent;

						// clear previous entered sensor reading
						gnd::matrix::set_zero(&reflect_prevent);

						// ---> scanning loop for sokuikiraw-data
						for(size_t i = 0; i < sokuikiraw.numPoints(); i++){
							// ---> entry laser scanner reflection
							gnd::vector::fixed_column<4> reflect_csns, reflect_cgl;
							gnd::vector::fixed_column<3> ws3x1;
							gnd::matrix::fixed<3,3> ws3x3;

							// if range data is null because of no reflection
							if(sokuikiraw[i].status == ssm::laser::STATUS_NO_REFLECTION)	continue;
							// ignore error data
							else if(sokuikiraw[i].isError()) 	continue;
							else if(sokuikiraw[i].r < sokuikiraw_prop.distMin * 1.1)	continue;
							else if(sokuikiraw[i].r > sokuikiraw_prop.distMax * 0.9)	continue;

							{ // ---> compute laser scanner reading position on robot coordinate
								// set search position on sensor-coordinate
								reflect_csns[0] = sokuikiraw[i].r * ::cos(sokuikiraw[i].th);
								reflect_csns[1] = sokuikiraw[i].r * ::sin(sokuikiraw[i].th);
								reflect_csns[2] = 0;
								reflect_csns[3] = 1;


								// data decimation with distance threshold
								if( gnd_square(reflect_csns[0] - reflect_prevent[0]) + gnd_square(reflect_csns[1] - reflect_prevent[1]) < culling_sqdist ){
									continue;
								}
								else {
									// update previous entered data
									gnd::matrix::copy(&reflect_prevent, &reflect_csns);
								}

								// convert from sensor coordinate to robot coordinate
								gnd::matrix::prod(&coordm_sns2gl, &reflect_csns, &reflect_cgl);
							} // <--- compute laser scanner reading position on robot coordinate

							// data entry
							gnd::opsm::counting_map(&cnt_smmap, reflect_cgl[0], reflect_cgl[1]);

							// log
							if( llog_fp )	::fprintf(llog_fp, "%lf %lf\n", reflect_cgl[0], reflect_cgl[1]);
						} // <--- scanning loop for sokuikiraw-data

						// log
						if( llog_fp )	::fprintf(llog_fp, "\n" );
					} // <--- 3. entry laser scanner reading


					odo_prevloop = ssmlog_odometry.data();
					cnt_ls++;
					::fprintf(stderr, ".");
				} // <--- read ssm sokuikiraw
			} // <--- map initialization loop

			// ---> map build
			if( !pconf.ndt.value ){
				if( gnd::opsm::build_map(&smmap, &cnt_smmap, gnd_mm2dist(1)) < 0 ){
					::fprintf(stderr, "\x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: invalid map property\n");
				}
				else {
					::fprintf(stderr, "\n... \x1b[1mOK\x1b[0m success to build psm map\n");
				}
			}
			else if( pconf.ndt.value && gnd::opsm::build_ndt_map(&smmap, &cnt_smmap ) < 0 ){
				::fprintf(stderr, "\x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: invalid map property\n");
			}
			else {
				::fprintf(stderr, "\n... \x1b[1mOK\x1b[0m success to build ndt map\n");
			} // <--- map build
		} // <--- map initialization




		{ // ---> timer
			// set parameter-cycle
//			timer_operate.begin(CLOCK_REALTIME, param.cycle.value, -param.cycle.value);
//			timer_clock.begin(CLOCK_REALTIME,
//					param.cycle.value < ClockCycle ? param.cycle.value :  ClockCycle);
			::fprintf(stderr, "\n");
			if( pconf.cui_show.value ) {
				timer_show.begin(CLOCK_REALTIME, ShowCycle, -ShowCycle);
				// console clear
				nline_show = 0;
			}
			else {
				// console clear
				::fprintf(stderr, "-------------------- cui mode --------------------\n");
				::fprintf(stderr, "  > ");
			}
		} // <--- timer



		// ---> operation loop
		while ( !::is_proc_shutoff() ) {
//			timer_clock.wait();

			{ // ---> cui
				int cuival = 0;
				char cuiarg[512];
				// zero reset buffer
				::memset(cuiarg, 0, sizeof(cuiarg));

				// ---> get command
				if( gcui.poll(&cuival, cuiarg, sizeof(cuiarg), cuito) > 0 ){
					if( timer_show.cycle() > 0 ){
						// quit show status mode
						timer_show.end();
						nline_show = 0;
						::fprintf(stderr, "-------------------- cui mode --------------------\n");
					}
					else { // ---> cui command operation
						switch(cuival){
						// exit
						case 'Q': ::proc_shutoff(); break;
						// help
						default:
						case '\0':
						case 'h': gcui.show(stderr, "   "); break;
						// show status
						case 's': {
							// console clear
							timer_show.begin(CLOCK_REALTIME, ShowCycle, -ShowCycle);
							break;
						}
						case 'f': {
							double freq = ::strtod(cuiarg, 0);
							if( freq <= 0 ){
								::fprintf(stderr, "   ... \x1b[31m\x1b[1mError\x1b[0m\x1b[39m: invalid argument value (frequency 0)\n");
								::fprintf(stderr, "       if you want to stop estimator, send \"\x1b[4mstand-by\x1b[0m\" command\n");
							}
							else {
								double cyc = 1.0 / freq;
								::fprintf(stderr, "   ... cycle %.03lf\n", cyc);
							}
						} break;

						// set freq
						case 'c': {
							double cyc = ::strtod(cuiarg, 0);
							if( cyc <= 0 ){
								::fprintf(stderr, "   ... \x1b[31m\x1b[1mError\x1b[0m\x1b[39m: invalid argument value (frequency 0)\n");
								::fprintf(stderr, "       if you want to stop estimator, send \"\x1b[4mstand-by\x1b[0m\" command\n");
							}
							else {
								::fprintf(stderr, "   ... cycle %.03lf\n", cyc);
							}
						} break;

						// start
						case 't':{
							cuito = 0.0;
						} break;
						// stand-by
						case 'B':{
							::fprintf(stderr, "   stand-by mode\n");
							cuito = -1;
						} break;
						}
					} // <--- cui command operation
					::fprintf(stderr, "  > ");
					gcui.poll(&cuival, cuiarg, sizeof( cuiarg ), 0);
				} // <--- get command
			}  // <--- cui


			// ---> show status
			if( timer_show.clock() > 0){
				// back cursor
				if( nline_show ) {
					::fprintf(stderr, "\x1b[%02dA", nline_show);
					nline_show = 0;
				}

				nline_show++; ::fprintf(stderr, "\x1b[K-------------------- \x1b[1m\x1b[36m%s\x1b[39m\x1b[0m --------------------\n", opsm_pt::proc_name);
				nline_show++; ::fprintf(stderr, "\x1b[K matching method : %s\n", pconf.ndt.value ? "ndt" : "psm");
				nline_show++; ::fprintf(stderr, "\x1b[K       optimizer : %s\n", pconf.optimizer.value );
				nline_show++; ::fprintf(stderr, "\x1b[K            loop : %d\n", cnt);
				nline_show++; ::fprintf(stderr, "\x1b[K   optimize loop : %d\n", cnt_opt);
				nline_show++; ::fprintf(stderr, "\x1b[K      likelihood : %.03lf\n", lkl );
				nline_show++; ::fprintf(stderr, "\x1b[K      position : %4.03lf[m], %4.03lf[m], %4.02lf[deg]\n",
						pos.x, pos.y, gnd_ang2deg( pos.theta ) );
				nline_show++; ::fprintf(stderr, "\x1b[K      move est : %4.03lf[m], %4.03lf[m], %4.02lf[deg]\n",
						move_est.x, move_est.y, gnd_ang2deg( move_est.theta ) );
//				::fprintf(stderr, "      cycle : %.03lf\n", timer_operate.cycle() );
				nline_show++; ::fprintf(stderr, "\x1b[K     optimizer : %s\n", ret == 0 ? "success" : "failure" );
				nline_show++; ::fprintf(stderr, "\x1b[K    map update : %d\n", cnt_mapupdate );
				nline_show++; ::fprintf(stderr, "\x1b[K matching fail : %d\n", cnt_fail );
				nline_show++; ::fprintf(stderr, "\x1b[K\n");
				nline_show++; ::fprintf(stderr, "\x1b[K Push \x1b[1mEnter\x1b[0m to change CUI Mode\n");
			} // <--- show status




			// ---> read ssm-sokuikiraw-data
			if( ssmlog_sokuikiraw.readNext()) {
				// ---> position tracking
				// ... operation flow
				//      *0. get laser scanner reading
				//       1. compute position estimation by odometry (on global coordinate)
				//		 2. set position estimation by odometry to optimization starting value
				//		 3. optimization iteration by matching laser scanner reading to map(likelihood field)
				//		 4. optimization error test and write position ssm-data
				//		 5. robot current position coordinate and odometory position coordinate
				//		 6. update map and view data


				{  // ---> 1. compute position estimation from odometry
					double dt;
					// get position on odometry position (on odometry cooordinate)
					if( !ssmlog_odometry.readTime(ssmlog_sokuikiraw.time()) ) continue;

					dt = ssmlog_odometry.time() - prev_time;
					prev_time = ssmlog_odometry.time();
					move_dist_pause += ssmlog_odometry.data().v * dt;
					move_orient_puase += ssmlog_odometry.data().w * dt;
					move_dist_map += ssmlog_odometry.data().v * dt;
					move_orient_map += ssmlog_odometry.data().w * dt;


					if( cnt_correct >= pconf.ini_match_cnt.value &&
						move_dist_pause * move_dist_pause < pconf.pause_dist.value * pconf.pause_dist.value &&
						move_orient_puase < ::fabs(pconf.pause_orient.value) )
						continue;

                    ssm::ScanPoint2D::_ssmRead( ssmlog_sokuikiraw.data(), &sokuikiraw, 0);

					{ // ---> compute the movement estimation
						gnd::matrix::fixed<4,1> odov_cprev;			// current odometry position vector on previous odometry coordinate

						{ // ---> compute current odometry position on previous odometry coordinate
							gnd::matrix::fixed<4,4> coordm_r2podo;		// coordinate matrix of previous odometry position
							gnd::matrix::fixed<4,1> ws4x1;

							// get previous odometry coordinate matrix
							coordtree.get_convert_matrix(0, coordid_odm, &coordm_r2podo);

							// multiply previous odometry coordinate matrix with current position vector
							ws4x1[0][0] = ssmlog_odometry.data().x;
							ws4x1[1][0] = ssmlog_odometry.data().y;
							ws4x1[2][0] = 0;
							ws4x1[3][0] = 1;
							gnd::matrix::prod(&coordm_r2podo, &ws4x1, &odov_cprev);
						} // <--- compute current odometry position on previous odometry coordinate

						// get movement estimation by odometry
						move_est.x = odov_cprev[0][0];
						move_est.y = odov_cprev[1][0];
						move_est.theta = ssmlog_odometry.data().theta - odo_prevloop.theta;
					} // <--- compute the movement estimation


					{ // ---> add movement estimation
						gnd::matrix::fixed<4,1> pos_odmest;

						{ // ---> compute position estimation by odometry on global coordinate
							gnd::matrix::fixed<4,4> coordm_rbt2gl;		// coordinate convert matrix from robot to global
							gnd::matrix::fixed<4,1> ws4x1;

							// set search position on sensor-coordinate
							coordtree.get_convert_matrix(coordid_rbt, coordid_gl, &coordm_rbt2gl);

							ws4x1[0][0] = move_est.x;
							ws4x1[1][0] = move_est.y;
							ws4x1[2][0] = 0;
							ws4x1[3][0] = 1;

							gnd::matrix::prod(&coordm_rbt2gl, &ws4x1, &pos_odmest);
						} // <--- compute position estimation by odometry on global coordinate

						// set position
						pos.x = pos_odmest[0][0];
						pos.y = pos_odmest[1][0];
						pos.theta += move_est.theta;

						optimizer->initial_parameter_set_position( optim_ini, pos.x, pos.y, pos.theta );
					} // <--- add movement estimation
				}  // <--- 1. compute position estimation from odometry



				// ---> 2. set position estimation by odometry to optimization starting value
				optimizer->begin(optim_ini);


				gnd::matrix::set_zero(&move_opt);
				{ // ---> 3. optimization iteration by matching laser scanner reading to map(likelihood field)
					double left_timer = 1.0;
					gnd::matrix::fixed<3,1> delta;
					gnd::vector::fixed_column<2> reflect_prev;

					// clear previous entered sensor reading
					gnd::matrix::set_zero(&reflect_prev);

					// ---> scanning loop for sokuikiraw-data
					for(size_t i = 0; i < sokuikiraw.numPoints(); i++){
						// ---> entry laser scanner reflection
						gnd::vector::fixed_column<4> reflect_csns, reflect_crbt;
						gnd::matrix::fixed<3,1> ws3x1;
						gnd::matrix::fixed<3,3> ws3x3;

						// if range data is null because of no reflection
						if( sokuikiraw[i].status == ssm::laser::STATUS_NO_REFLECTION)	continue;
						// ignore error data
						else if( sokuikiraw[i].isError()) 	continue;
						else if( sokuikiraw[i].r < sokuikiraw_prop.distMin * 1.1)	continue;
						else if( sokuikiraw[i].r > sokuikiraw_prop.distMax * 0.9)	continue;

						{ // ---> compute laser scanner reading position on robot coordinate
							// set search position on sensor-coordinate
							reflect_csns[0] = sokuikiraw[i].r * ::cos( sokuikiraw[i].th );
							reflect_csns[1] = sokuikiraw[i].r * ::sin( sokuikiraw[i].th );
							reflect_csns[2] = 0;
							reflect_csns[3] = 1;


							// data decimation with distance threshold
							if( gnd_square(reflect_csns[0] - reflect_prev[0]) + gnd_square(reflect_csns[1] - reflect_prev[1]) < culling_sqdist ){
								continue;
							}
							else {
								// update previous entered data
								gnd::matrix::copy(&reflect_prev, &reflect_csns);
							}

							// convert from sensor coordinate to robot coordinate
							gnd::matrix::prod(&coordm_sns2rbt, &reflect_csns, &reflect_crbt);
						} // <--- compute laser scanner reading position on robot coordinate

						// data entry
						optimizer->set_scan_point( reflect_crbt[0] , reflect_crbt[1] );
						// <--- entry laser scanner reflection
					} // <--- scanning loop for sokuikiraw-data

					// zero reset likelihood
					lkl = 0;
					// zero reset optimization iteration counter
					cnt_opt = 0;
					gnd::matrix::set_zero(&move_opt);
					do{
						// store previous optimization position likelihood

						{ // ---> step iteration of optimization
							gnd::matrix::fixed<3,1> ws3x1;
							if( (ret = optimizer->iterate(&delta, &ws3x1, &lkl)) < 0 ){
								break;
							}

							// get optimized position
							pos_opt.x = ws3x1[0][0];
							pos_opt.y = ws3x1[1][0];
							pos_opt.theta = ws3x1[2][0];
							// get movement by optimization
							gnd::matrix::add(&move_opt, &delta, &move_opt);
						} // <--- step iteration of optimization

						// loop counting
						cnt_opt++;
						// convergence test
					} while( !optimizer->converge_test() && left_timer > gnd_msec2time(30) ); // <--- position optimization loop
				} // ---> 3. optimization iteration by matching laser scanner reading to map(likelihood field)



				// ---> 4. optimization error test and write position ssm-data
				// check --- 1st. function error, 2nd. distance, 3rd. orient difference
				if( ret >= 0 &&
						gnd_square( pos.x - pos_opt.x ) + gnd_square( pos.y - pos_opt.y ) < gnd_square( pconf.failure_dist.value ) &&
						::fabs( pos.theta - pos_opt.theta ) < pconf.failure_orient.value ) {



					pos.x = pos_opt.x;
					pos.y = pos_opt.y;
					pos.theta = pos_opt.theta;
					cnt_correct++;


					if( tlog_fp ){
						::fprintf(tlog_fp, "%lf %lf %lf %lf %lf %lf %lf %lf %lf %lf %lf\n",
								ssmlog_odometry.time() - init_time,
								ssmlog_odometry.data().x, ssmlog_odometry.data().y, ssmlog_odometry.data().theta,
								ssmlog_odometry.data().v, ssmlog_odometry.data().w,
								pos_opt.x, pos_opt.y, pos_opt.theta,
								pos_opt.v, pos_opt.w );
					}

					if( t4re_fp ) {
						::fprintf(t4re_fp, "A %lf %lf\n",
								pos_opt.x, pos_opt.y );
					}

					if( cnt_correct >= pconf.ini_match_cnt.value ) {
						gnd::odometry::correction::counting(&cmap, pos_opt.x, pos_opt.y, pos_opt.theta,
								move_dist_pause, (pos.x - pos_opt.x), (pos.y - pos_opt.y), (pos.theta - pos_opt.theta) );
					}
					move_dist_pause = 0;
					move_orient_puase = 0;
				}
				else {
					cnt_fail++;
//					ssmlog_pos.write( ssmlog_sokuikiraw.time() );
				} // <--- 4. optimization error test and write position ssm-data


				{ // ---> 5. update scan matching result coordinate
					gnd::matrix::fixed<4,4> coordm;

					// odometry coordinate
					gnd::matrix::coordinate_converter(&coordm,
							ssmlog_odometry.data().x, ssmlog_odometry.data().y, 0,
							::cos(ssmlog_odometry.data().theta), ::sin(ssmlog_odometry.data().theta), 0,
							 0, 0, 1);
					coordtree.set_coordinate(coordid_odm, &coordm);

					// robot position coordinate
					gnd::matrix::coordinate_converter(&coordm,
							pos.x, pos.y, 0,
							::cos( pos.theta ), ::sin( pos.theta ), 0,
							 0, 0, 1);
					coordtree.set_coordinate(coordid_rbt, &coordm);

					// get coordinate convert matrix
					coordtree.get_convert_matrix(coordid_sns, coordid_gl, &coordm_sns2gl);
				} // ---> 5. update robot position coordinate and odometory position coordinate



				{ // ---> 6. update map and view data
					// map update check, 1st. time, 2nd. position, 3rd. orient
					mapupdate = ssmlog_sokuikiraw.time() - time_premap > pconf.map_update_time.value ||
							gnd_square( pos.x - pos_premap.x) + gnd_square( pos.y - pos_premap.y) > gnd_square(pconf.map_update_dist.value) ||
							::fabs( pos.theta - pos_premap.theta ) > pconf.map_update_orient.value;

					if( mapupdate ) cnt_mapupdate++;
					if( mapupdate && !pconf.map_update.value ){ // ---> clear
						gnd::opsm::clear_counting_map(&cnt_smmap);
					} // <--- clear

					{// ---> scanning loop for sokuikiraw-data
						gnd::vector::fixed_column<4> reflect_csns;
						gnd::vector::fixed_column<4> reflect_cgl;
						gnd::vector::fixed_column<2> reflect_prevent;

						// ---> scanning loop of laser scanner reading
						for(size_t i = 0; i < sokuikiraw.numPoints(); i++){
							gnd::matrix::fixed<3,1> ws3x1;
							gnd::matrix::fixed<3,3> ws3x3;

							// if range data is null because of no reflection
							if( sokuikiraw[i].status == ssm::laser::STATUS_NO_REFLECTION)	continue;
							// ignore error data
							else if( sokuikiraw[i].isError()) 	continue;
							else if( sokuikiraw[i].r < sokuikiraw_prop.distMin)	continue;
							else if( sokuikiraw[i].r > sokuikiraw_prop.distMax)	continue;

							{ // ---> compute laser scanner reading position on global coordinate
								// set search position on sensor-coordinate
								gnd::matrix::set(&reflect_csns, 0, 0, sokuikiraw[i].r * ::cos( sokuikiraw[i].th ));
								gnd::matrix::set(&reflect_csns, 1, 0, sokuikiraw[i].r * ::sin( sokuikiraw[i].th ) );
								gnd::matrix::set(&reflect_csns, 2, 0, 0);
								gnd::matrix::set(&reflect_csns, 3, 0, 1);

								// data decimation with distance threshold
								if( gnd_square(reflect_csns[0] - reflect_prevent[0]) + gnd_square(reflect_csns[1] - reflect_prevent[1]) < culling_sqdist ){
									continue;
								}
								else {
									// update previous entered data
									gnd::matrix::copy(&reflect_prevent, &reflect_csns);
								}

								// convert from sensor coordinate to global coordinate
								gnd::matrix::prod(&coordm_sns2gl, &reflect_csns, &reflect_cgl);
							} // <--- compute laser scanner reading position on global coordinate

							// ---> enter laser scanner reading to map
							if( mapupdate ) {
								if( pconf.map_update.value ){
									if( pconf.ndt.value ) {
										gnd::opsm::update_ndt_map(&cnt_smmap, &smmap, reflect_cgl[0], reflect_cgl[1], gnd_mm2dist(1));
									}
									else {
										gnd::opsm::update_map(&cnt_smmap, &smmap, reflect_cgl[0], reflect_cgl[1], gnd_mm2dist(1) );
									}
								}
								else {
									gnd::opsm::counting_map(&cnt_smmap, reflect_cgl[0], reflect_cgl[1]);
								}
								// update
								time_premap = ssmlog_sokuikiraw.time();
								pos_premap = pos;
							} // <--- enter laser scanner reading to map

							// log
							if( llog_fp )	::fprintf(llog_fp, "%lf %lf\n", reflect_cgl[0], reflect_cgl[1]);
						} // <--- scanning loop of laser scanner reading

						// log
						if( llog_fp )	::fprintf(llog_fp, "\n" );

					} // <--- scanning loop for sokuikiraw-data


					// ---> rebuild map
					if( mapupdate ) {
						if( pconf.map_update.value ){
						}
						else {
							if( !pconf.ndt.value ){
								if( gnd::opsm::build_map(&smmap, &cnt_smmap, gnd_mm2dist(1)) < 0 ){
									::fprintf(stderr, "\x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: invalid map property\n");
								}
							}
							else{
								if( gnd::opsm::build_ndt_map(&smmap, &cnt_smmap, gnd::opsm::ErrorMargin) < 0 ){
									::fprintf(stderr, "\x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: invalid map property\n");
								}
							}
						}
					} // <--- rebuild map
				} // ---> 6. update map and view data
				odo_prevloop = ssmlog_odometry.data();
				cnt++;
			} // <--- read ssm sokuikiraw

		} // <--- operation loop

	} // <--- operation



	{ // ---> finalization
		if(tlog_fp) ::fclose(tlog_fp);
		if(llog_fp) ::fclose(llog_fp);
		if(t4re_fp) ::fclose(t4re_fp);

		optimizer->initial_parameter_delete(&optim_ini);
		delete optimizer;

		// slam
		if( pconf.map_update.value ) {

			// ---> build map
			if( pconf.ndt.value ) {
				gnd::opsm::build_ndt_map(&smmap, &cnt_smmap );
			}
			else {
				gnd::opsm::build_map(&smmap, &cnt_smmap, gnd_mm2dist(10));
			} // <--- build map

			if( pconf.opsm_map.value[0] ){ // ---> write opsm map
				char dname[512];
				bool flg = false;

				::fprintf(stderr, " => write intermediate file\n");
				::sprintf(dname, "%s/%s/", *pconf.output_dir.value ? pconf.output_dir.value : "./", pconf.opsm_map.value);

				errno = 0;
				if( mkdir(dname, S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IROTH ) < 0) {
					if( errno != EEXIST ) {
						::proc_shutoff();
						::fprintf(stderr, " ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: fail to make output directory \"\x1b[4m%s\x1b[0m\"\n", dname);
					}
					else {
						struct stat st;
						::stat(pconf.output_dir.value, &st);

						if( S_ISDIR(st.st_mode)) {
							::fprintf(stderr, " ...\x1b[1mOK\x1b[0m: output directory \"\x1b[4m%s\x1b[0m\" is already exist\n", dname);
							flg = true;
						}
						else {
							::proc_shutoff();
							::fprintf(stderr, " ... \x1b[1m\x1b[31mERROR\x1b[39m\x1b[0m: \"\x1b[4m%s\x1b[0m\" is already exist and it is not directory\n", dname);
						}
					}
				}
				else {
					::fprintf(stderr, "  ... make output directory \"\x1b[4m%s\x1b[0m\"\n", dname);
					flg = true;
				}


				if( flg && gnd::opsm::write_counting_map(&cnt_smmap, dname) ) {
					::fprintf(stderr, "  ... \x1b[1m\x1b[31mError\x1b[39m\x1b[0m: fail to open\n");
				}
				else {
					::fprintf(stderr, "  ... \x1b[1mOK\x1b[0m: save counting map data\n");
				}
			} // <--- write opsm map



			if( pconf.bmp.value ) { // ---> bmp (32bit)
				gnd::bmp32_t bmp;

				// bmp file building
				gnd::opsm::build_bmp32(&bmp, &smmap, gnd_m2dist( 1.0 / 10));
				{ // ---> bmp
					char fname[512];
					::fprintf(stderr, " => write psm-image in bmp(32bit)\n");

					if( ::snprintf(fname, sizeof(fname), "%s/%s.%s", pconf.output_dir.value, "out", "bmp" ) == sizeof(fname) ){
						::fprintf(stderr, "  ... \x1b[1m\x1b[31mError\x1b[39m\x1b[0m: fail to open. file name is too long\n");
					}
					else if( gnd::bmp::write32(fname, &bmp) < 0) {
						::fprintf(stderr, "  ... \x1b[1m\x1b[31mError\x1b[39m\x1b[0m: fail to open \"\x1b[4m%s\x1b[0m\"\n", fname);
					}
					else {
						::fprintf(stderr, "  ... \x1b[1mOK\x1b[0m: save map data into \"\x1b[4m%s\x1b[0m\"\n", fname);
					}
				} // <--- bmp

				{ // ---> origin
					char fname[512];
					FILE *fp = 0;
					double x, y;

					if( ::snprintf(fname, sizeof(fname), "%s/%s.%s", pconf.output_dir.value, "out", "origin.txt"  ) == sizeof(fname) ){
						::fprintf(stderr, "  ... \x1b[1m\x1b[31mError\x1b[39m\x1b[0m: fail to open. file name is too long\n");
					}
					else if( !(fp = fopen(fname, "w")) ) {
						::fprintf(stderr, "  ... \x1b[1m\x1b[31mError\x1b[39m\x1b[0m: fail to open \"\x1b[4m%s\x1b[0m\"\n", fname);
					}

					bmp.pget_origin(&x, &y);
					fprintf(fp, "%lf %lf\n", x, y);
					fclose(fp);
				} // --->  origin
			} // <--- bmp (32bit)


			if( pconf.bmp.value ) { // ---> bmp (8bit)
				gnd::bmp8_t bmp8;

				gnd::opsm::build_bmp8(&bmp8, &smmap, gnd_m2dist( 1.0 / 10));
				{ // ---> bmp
					char fname[512];
					::fprintf(stderr, " => write psm-image in bmp(8bit)\n");

					if( ::snprintf(fname, sizeof(fname), "%s/%s.%s", pconf.output_dir.value, "out8", "bmp" ) == sizeof(fname) ){
						::fprintf(stderr, "  ... \x1b[1m\x1b[31mError\x1b[39m\x1b[0m: fail to open. file name is too long\n");
					}
					else if( gnd::bmp::write8(fname, &bmp8) < 0) {
						::fprintf(stderr, "  ... \x1b[1m\x1b[31mError\x1b[39m\x1b[0m: fail to open \"\x1b[4m%s\x1b[0m\"\n", fname);
					}
					else {
						::fprintf(stderr, "  ... \x1b[1mOK\x1b[0m: save map data into \"\x1b[4m%s\x1b[0m\"\n", fname);
					}
				} // <--- bmp

				{ // ---> origin
					char fname[512];
					FILE *fp = 0;
					double x, y;

					if( ::snprintf(fname, sizeof(fname), "%s/%s.%s", pconf.output_dir.value, "out8", "origin.txt"  ) == sizeof(fname) ){
						::fprintf(stderr, "  ... \x1b[1m\x1b[31mError\x1b[39m\x1b[0m: fail to open. file name is too long\n");
					}
					else if( !(fp = fopen(fname, "w")) ) {
						::fprintf(stderr, "  ... \x1b[1m\x1b[31mError\x1b[39m\x1b[0m: fail to open \"\x1b[4m%s\x1b[0m\"\n", fname);
					}
					bmp8.pget_origin(&x, &y);
					fprintf(fp, "%lf %lf\n", x, y);
					fclose(fp);
				} // --->  origin
			} // ---> bmp (8bit)
		}


		if(pconf.debug_odo_err_map.value) { // ---> file out
			gnd::odometry::correction::vxl *p;
			double x, y, t;
			char fname[128];
			char mapfname[128];

			for( unsigned int zi = 0; zi < cmap.zsize(); zi++ ) {
				FILE *fp;
				double uz, lz;
				::sprintf(fname, "map%d.dat", zi);
				fp = ::fopen(fname, "w");
				cmap.sget_pos_lower(0, 0, zi, 0, 0, &lz);
				cmap.sget_pos_upper(0, 0, zi, 0, 0, &uz);
				::fprintf(fp, "# angular range %lf %lf\n", lz, uz);

				::sprintf(mapfname, "map%d.rmap", zi);
				// ---> row
				for( unsigned int yi = 0; yi < cmap.ysize(); yi++ ) {
					// ---> column
					for( unsigned int xi = 0; xi < cmap.xsize(); xi++ ) {
						p = cmap.pointer(xi, yi, zi);
						cmap.sget_pos_core(xi,yi,zi, &x, &y, &t);
						::fprintf( fp, "%lf %lf, %lf %lf %lf, %lf\n", x, y,
								p->dist > 0.01 ? p->dx / p->dist : 0, p->dist > 0.01 ? p->dy / p->dist : 0, p->dist > 0.01 ? gnd_ang2deg(p->dtheta / p->dist) : 0,
								p->dist);
					} // <--- column
				} // <--- row
				::fclose(fp);
			}

			cmap.fwrite("odometry-correction.cmap");
//			gnd::odometry::rsemap::write("rse-map", &cmap);
		} // <--- file out

		::fprintf(stderr, "\n");
		::fprintf(stderr, "Finish\n");

	} // <--- finalization

	return 0;
}





