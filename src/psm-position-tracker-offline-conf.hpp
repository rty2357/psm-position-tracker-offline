/*
 * psm-position-tracker-offline-conf.hpp
 *
 *  Created on: 2011/12/22
 *      Author: tyamada
 */

#ifndef PSM_POSITION_TRACKER_OFFLINE_CONF_HPP_
#define PSM_POSITION_TRACKER_OFFLINE_CONF_HPP_

#include <ssmtype/spur-odometry.h>
#include "ssm-laser.hpp"

#include "gnd-psm.hpp"
#include "gnd-config-file.hpp"
#include "gnd-lib-error.h"

#ifndef psm_pt
#define psm_pt psm::position_tracker
#endif

#define __OPTIMIZER_NEWTON__		"newton"
#define __OPTIMIZER_QMC__	 		"qmc"
#define __OPTIMIZER_QMC2NEWTON__	"qmc2newton"


// ---> constant definition
namespace psm {
	namespace position_tracker {
		static const char proc_name[] = "psm-position-tracker-offline";

		// map-file
		static const gnd::conf::parameter_array<char, 512> ConfIni_ScanMatchingMapDir = {
				"scan-matching-map",
				"",		// map file directory
				"scan matching map directory path (optional argument)"
		};

		// map-file
		static const gnd::conf::parameter_array<char, 512> ConfIni_CorrectionMapPath = {
				"correction-map",
				"",		// map path
				"correction map file path (optional argument)"
		};

		// ssm-name
		static const gnd::conf::parameter_array<char, 512> ConfIni_OdometryLogName = {
				"odm-log-name",
				"",
				"odometry log file path"
		};

		// laser scanner ssm-name
		static const gnd::conf::parameter_array<char, 512> ConfIni_LaserScannerLogName = {
				"laser-scanner-log-name",
				"",
				"laser scanner log file path"
		};
		// decimate threshold
		static const gnd::conf::parameter<double> ConfIni_Culling = {
				"culling",
				gnd_m2dist( 0.08 ),	// [m]
				"distance threshold of scan data culling [m]"
		};

		// failure test parameter threshold
		static const gnd::conf::parameter<double> ConfIni_FailDist = {
				"fail-test-dist",
				gnd_m2dist(1.0),
				"distance threshold of scan matching failure test [m]"
		};

		// failure test parameter orient
		static const gnd::conf::parameter<double> ConfIni_FailOrient = {
				"fail-test-orient",
				gnd_deg2ang(10),
				"orient threshold of scan matching failure test [deg]"
		};


		// cycle
		static const gnd::conf::parameter<double> ConfIni_Cycle = {
				"cycle",
				gnd_sec2time(0.2),	// [s]
				"scan matching cycle [sec]",
		};

		// rest-cycle
		static const gnd::conf::parameter<double> ConfIni_RestCycle = {
				"rest-cycle",
				gnd_sec2time(10.0),	// [s]
		};
		// rest-threshold-distance
		static const gnd::conf::parameter<double> ConfIni_RestDist = {
				"rest-threshold-distance",
				gnd_m2dist(0.05),	// [m]
		};
		// rest-threshold-orientation
		static const gnd::conf::parameter<double> ConfIni_RestOrient = {
				"rest-threshold-orientation",
				gnd_deg2ang(2.5),	// [rad]
		};

		// rest-threshold-orientation
		static const gnd::conf::parameter<bool> ConfIni_SLAM = {
				"slam",
				false,
				"switch of slam (map building)",
		};
		// map update parameter time
		static const gnd::conf::parameter<double> ConfIni_MapUpdateTime = {
				"map-update-time",
				gnd_sec2time(30),
				"time threshold of map update [sec]"
		};

		// map update parameter threshold
		static const gnd::conf::parameter<double> ConfIni_MapUpdateDist = {
				"map-update-dist",
				gnd_m2dist(0.1),
				"distance threshold of map update [m]"
		};

		// map update parameter orient
		static const gnd::conf::parameter<double> ConfIni_MapUpdateOrient = {
				"map-update-orient",
				gnd_deg2ang(30),
				"orient threshold of map update [deg]"
		};


		// optimizer
		static const char OptNewton[]		= __OPTIMIZER_NEWTON__;
		static const char OptQMC[]			= __OPTIMIZER_QMC__;
		static const char OptQMC2Newton[]	= __OPTIMIZER_QMC2NEWTON__;
		static const gnd::conf::parameter_array<char, 512> ConfIni_Optimizer = {
				"optimizer",
				__OPTIMIZER_NEWTON__,		// map file directory
				"optimize method (newton or qmc or qmc2newton)"
		};

		// distance threshold of converge test
		static const gnd::conf::parameter<double> ConfIni_ConvergeDist = {
				"converge-distance",
				gnd_m2dist(0.01),	// [m]
				"distance threshold of converge test [m]",
		};
		// orient threshold of converge test
		static const gnd::conf::parameter<double> ConfIni_ConvergeOrient = {
				"converge-orient",
				gnd_deg2ang(0.5),	// [rad]
				"orient threshold of converge test [deg]",
		};

		// number of scan data for first map building
		static const gnd::conf::parameter<int> ConfIni_InitMapCnt = {
				"init-map-cnt",
				1,	// [deg]
				"number of scan data for initial map building",
		};

		// number of scan data for first map building
		static const gnd::conf::parameter<int> ConfIni_InitMatchingCnt = {
				"init-matching-cnt",
				5,	// [deg]
				"count of initial position estimation. in these matching result is not resister on odometry error map",
		};

		// rest-threshold-orientation
		static const gnd::conf::parameter<bool> ConfIni_NDT = {
				"ndt",
				false,
				"scan matching evaluation function (ndt or op)"
		};


		// debug viewer switch
		static const gnd::conf::parameter<bool> ConfIni_DebugShowMode = {
				"debug-show-mode",
				true,
		};



		// number of scan data for first map building
		static const gnd::conf::parameter<double> ConfIni_PosGridSizeX = {
				"correction-map-pos-grid-x",
				1.0,	// [m]
		};

		// number of scan data for first map building
		static const gnd::conf::parameter<double> ConfIni_PosGridSizeY = {
				"correction-map-pos-grid-y",
				1.0,	// [m]
		};

		// number of scan data for first map building
		static const gnd::conf::parameter<int> ConfIni_AngReslution = {
				"correction-map-ang-rsl",
				8,	// [m]
		};

	} // <--- namespace psm
} // <--- namespace position_tracker
// <--- constant definition


// ---> type declaration
namespace psm {
	namespace position_tracker {
		struct proc_configuration;
		typedef struct proc_configuration proc_configuration;
	}
}
// ---> type declaration


// ---> function declaration
namespace psm {
	namespace position_tracker {
		/*
		 * @brief initialize configure to default parameter
		 */
		int proc_conf_initialize(proc_configuration *conf);

		/*
		 * @brief get configuration parameter
		 */
		int proc_conf_get( gnd::conf::configuration* src, proc_configuration* dest );
		/*
		 * @brief set configuration parameter
		 */
		int proc_conf_set( gnd::conf::configuration* dest, proc_configuration* src );

		/*
		 * @brief read configuration parameter
		 */
		int proc_conf_read( const char* f, proc_configuration* dest );
		/*
		 * @brief write configuration parameter
		 */
		int proc_conf_write( const char* f, proc_configuration* src );

	}
}
// ---> function declaration



// ---> type definition
namespace psm {
	namespace position_tracker {
		/**
		 * \brief particle localizer configure
		 */
		struct proc_configuration {
			proc_configuration();

			gnd::conf::parameter_array<char, 512>	smmapdir;			///< scan matching map directory path
			gnd::conf::parameter_array<char, 512>	cmap;				///< correction map path
			gnd::conf::parameter_array<char, 512>	odm_logname;		///< odometry position estimation ssm-name
			gnd::conf::parameter_array<char, 512>	ls_logname;			///< laser scanner data ssm-name
			gnd::conf::parameter<double>			culling;			///< laser scanner data decimate parameter [m]
			gnd::conf::parameter<double>			cycle;				///< operation cycle
			gnd::conf::parameter<double>			fail_dist;			///< failure test parameter (distance threshold)
			gnd::conf::parameter<double>			fail_orient;			///< failure test parameter (orient threshold)

			gnd::conf::parameter<double>			rest_cycle;			///< resting mode cycle
			gnd::conf::parameter<double>			rest_dist;			///< resting threshold (position distance) [m]
			gnd::conf::parameter<double>			rest_orient;		///< resting threshold (position orientation) [deg]
			gnd::conf::parameter<bool>				slam;				///< slam mode flag
			gnd::conf::parameter<double>			mapupdate_time;		///< map update parameter (time threshold)
			gnd::conf::parameter<double>			mapupdate_dist;		///< map update parameter (distance threshold)
			gnd::conf::parameter<double>			mapupdate_orient;	///< map update parameter (orient threshold)


			gnd::conf::parameter_array<char, 512>	optimizer;			///< kind of optimizer
			gnd::conf::parameter<double>			converge_dist;		///< convergence test threshold (position distance) [m]
			gnd::conf::parameter<double>			converge_orient;	///< convergence test threshold (position orientation) [deg]
			gnd::conf::parameter<int>				ini_map_cnt;		///< number of scan data for first map building
			gnd::conf::parameter<int>				ini_match_cnt;		///< count of initial position estimation. in these matching result is not resister on odometry error map
			gnd::conf::parameter<bool>				ndt;				///< ndt mode
			gnd::conf::parameter<bool>				debug_show;			///< debug show mode

			gnd::conf::parameter<double>			pos_gridsizex;		///< road map position size
			gnd::conf::parameter<double>			pos_gridsizey;		///< road map position size
			gnd::conf::parameter<int>				ang_rsl;			///< road map angular resolution
		};

		/**
		 * @brief constructor
		 */
		inline
		proc_configuration::proc_configuration(){
			proc_conf_initialize(this);
		}

	}
}
// ---> type definition




// ---> function definition
namespace psm {
	namespace position_tracker {

		/**
		 * @brief initialize configure
		 */
		inline
		int proc_conf_initialize(proc_configuration *conf){
			gnd_assert(!conf, -1, "invalid null pointer");

			::memcpy(&conf->smmapdir,			&ConfIni_ScanMatchingMapDir,	sizeof(ConfIni_ScanMatchingMapDir) );
			::memcpy(&conf->cmap,				&ConfIni_CorrectionMapPath,		sizeof(ConfIni_CorrectionMapPath) );
			::memcpy(&conf->odm_logname,		&ConfIni_OdometryLogName,		sizeof(ConfIni_OdometryLogName) );
			::memcpy(&conf->ls_logname,			&ConfIni_LaserScannerLogName,	sizeof(ConfIni_LaserScannerLogName) );
			::memcpy(&conf->culling,			&ConfIni_Culling,				sizeof(ConfIni_Culling) );
			::memcpy(&conf->cycle,				&ConfIni_Cycle,					sizeof(ConfIni_Cycle) );
			::memcpy(&conf->fail_dist,			&ConfIni_FailDist,				sizeof(ConfIni_FailDist) );
			::memcpy(&conf->fail_orient,		&ConfIni_FailOrient,			sizeof(ConfIni_FailOrient) );
			::memcpy(&conf->rest_cycle,			&ConfIni_RestCycle,				sizeof(ConfIni_RestCycle) );
			::memcpy(&conf->rest_dist,			&ConfIni_RestDist,				sizeof(ConfIni_RestDist) );
			::memcpy(&conf->rest_orient,		&ConfIni_RestOrient,			sizeof(ConfIni_RestOrient) );
			::memcpy(&conf->slam,				&ConfIni_SLAM,					sizeof(ConfIni_SLAM) );
			::memcpy(&conf->mapupdate_time,		&ConfIni_MapUpdateTime,			sizeof(ConfIni_MapUpdateTime) );
			::memcpy(&conf->mapupdate_dist,		&ConfIni_MapUpdateDist,			sizeof(ConfIni_MapUpdateDist) );
			::memcpy(&conf->mapupdate_orient,	&ConfIni_MapUpdateOrient,		sizeof(ConfIni_MapUpdateOrient) );
			::memcpy(&conf->optimizer,			&ConfIni_Optimizer,				sizeof(ConfIni_Optimizer) );
			::memcpy(&conf->converge_dist,		&ConfIni_ConvergeDist,			sizeof(ConfIni_ConvergeDist) );
			::memcpy(&conf->converge_orient,	&ConfIni_ConvergeOrient,		sizeof(ConfIni_ConvergeOrient) );
			::memcpy(&conf->ini_map_cnt,		&ConfIni_InitMapCnt,			sizeof(ConfIni_InitMapCnt) );
			::memcpy(&conf->ini_match_cnt,		&ConfIni_InitMatchingCnt,		sizeof(ConfIni_InitMatchingCnt) );
			::memcpy(&conf->ndt,				&ConfIni_NDT,					sizeof(ConfIni_NDT) );
			::memcpy(&conf->debug_show,			&ConfIni_DebugShowMode,			sizeof(ConfIni_DebugShowMode) );

			::memcpy(&conf->pos_gridsizex,		&ConfIni_PosGridSizeX,			sizeof(ConfIni_PosGridSizeX) );
			::memcpy(&conf->pos_gridsizey,		&ConfIni_PosGridSizeY,			sizeof(ConfIni_PosGridSizeY) );
			::memcpy(&conf->ang_rsl,			&ConfIni_AngReslution,			sizeof(ConfIni_AngReslution) );
			return 0;
		}

		/**
		 * @brief get configuration parameter
		 * @param  [in] src  : configuration
		 * @param [out] dest : configuration parameter
		 */
		inline
		int proc_conf_get( gnd::conf::configuration* src, proc_configuration* dest )
		{
			gnd_assert(!src, -1, "invalid null pointer");
			gnd_assert(!dest, -1, "invalid null pointer");

			gnd::conf::get_parameter( src, &dest->smmapdir );
			gnd::conf::get_parameter( src, &dest->cmap );
			gnd::conf::get_parameter( src, &dest->culling );
			gnd::conf::get_parameter( src, &dest->cycle );
			gnd::conf::get_parameter( src, &dest->fail_dist );
			if( !gnd::conf::get_parameter( src, &dest->fail_orient) )
				dest->fail_orient.value = gnd_deg2ang(dest->fail_orient.value);
			gnd::conf::get_parameter( src, &dest->rest_cycle );
			gnd::conf::get_parameter( src, &dest->rest_dist );
			if( !gnd::conf::get_parameter( src, &dest->rest_orient ) )
				dest->rest_orient.value = gnd_deg2ang(dest->rest_orient.value);
			gnd::conf::get_parameter( src, &dest->slam );
			gnd::conf::get_parameter( src, &dest->mapupdate_time );
			gnd::conf::get_parameter( src, &dest->mapupdate_dist );
			if( !gnd::conf::get_parameter( src, &dest->mapupdate_orient) )
				dest->converge_orient.value = gnd_deg2ang(dest->mapupdate_orient.value);
			gnd::conf::get_parameter( src, &dest->optimizer );
			gnd::conf::get_parameter( src, &dest->converge_dist );
			if( !gnd::conf::get_parameter( src, &dest->converge_orient) )
				dest->converge_orient.value = gnd_deg2ang(dest->converge_orient.value);
			gnd::conf::get_parameter( src, &dest->ini_map_cnt );
			gnd::conf::get_parameter( src, &dest->ini_match_cnt );
			gnd::conf::get_parameter( src, &dest->ndt );
			gnd::conf::get_parameter( src, &dest->debug_show );
			gnd::conf::get_parameter( src, &dest->pos_gridsizex );
			gnd::conf::get_parameter( src, &dest->pos_gridsizey );
			gnd::conf::get_parameter( src, &dest->ang_rsl );

			gnd::conf::get_parameter( src, &dest->odm_logname );
			gnd::conf::get_parameter( src, &dest->ls_logname );
			return 0;
		}

		/**
		 * @brief file out  configure file
		 */
		inline
		int proc_conf_set( gnd::conf::configuration* dest, proc_configuration* src ) {

			gnd_assert(!dest, -1, "invalid null pointer");
			gnd_assert(!src, -1, "invalid null pointer");

			{ // ---> operation
				gnd::conf::set_parameter(dest, &src->smmapdir);
				gnd::conf::set_parameter(dest, &src->cmap );
				gnd::conf::set_parameter(dest, &src->culling);
				gnd::conf::set_parameter(dest, &src->cycle);
				gnd::conf::set_parameter(dest, &src->fail_dist);

				src->fail_orient.value = gnd_ang2deg(src->fail_orient.value);
				gnd::conf::set_parameter(dest, &src->fail_orient);
				src->fail_orient.value = gnd_deg2ang(src->fail_orient.value);

				gnd::conf::set_parameter(dest, &src->rest_cycle);
				gnd::conf::set_parameter(dest, &src->rest_dist);
				src->rest_orient.value = gnd_ang2deg(src->rest_orient.value);
				gnd::conf::set_parameter(dest, &src->rest_orient );
				src->rest_orient.value = gnd_deg2ang(src->rest_orient.value);
				gnd::conf::set_parameter(dest, &src->slam);
				gnd::conf::set_parameter(dest, &src->mapupdate_time);
				gnd::conf::set_parameter(dest, &src->mapupdate_dist);
				src->mapupdate_orient.value = gnd_ang2deg(src->mapupdate_orient.value);
				gnd::conf::set_parameter(dest, &src->mapupdate_orient);
				src->mapupdate_orient.value = gnd_deg2ang(src->mapupdate_orient.value);
				gnd::conf::set_parameter(dest, &src->optimizer);
				gnd::conf::set_parameter(dest, &src->converge_dist);
				src->converge_orient.value = gnd_ang2deg(src->converge_orient.value);
				gnd::conf::set_parameter(dest, &src->converge_orient);
				src->converge_orient.value = gnd_deg2ang(src->converge_orient.value);

				gnd::conf::set_parameter(dest, &src->ini_map_cnt );
				gnd::conf::set_parameter(dest, &src->ini_match_cnt );
				gnd::conf::set_parameter(dest, &src->ndt);
				gnd::conf::set_parameter(dest, &src->debug_show );

				gnd::conf::set_parameter(dest, &src->pos_gridsizex );
				gnd::conf::set_parameter(dest, &src->pos_gridsizey );

				gnd::conf::set_parameter(dest, &src->ang_rsl );


				gnd::conf::set_parameter(dest, &src->odm_logname);
				gnd::conf::set_parameter(dest, &src->ls_logname);

				return 0;
			} // <--- operation
		}


        /**
         * @brief read configuration parameter file
         * @param [in]  f    : configuration file name
         * @param [out] dest : configuration parameter
         */
        inline
        int proc_conf_read(const char* f, proc_configuration* dest) {
                gnd_assert(!f, -1, "invalid null pointer");
                gnd_assert(!dest, -1, "invalid null pointer");

                { // ---> operation
                        int ret;
                        gnd::conf::file_stream fs;
                        // configuration file read
                        if( (ret = fs.read(f)) < 0 )    return ret;

                        return proc_conf_get(&fs, dest);
                } // <--- operation
        }

        /**
         * @brief write configuration parameter file
         * @param [in]  f  : configuration file name
         * @param [in] src : configuration parameter
         */
        inline
        int proc_conf_write(const char* f, proc_configuration* src){
                gnd_assert(!f, -1, "invalid null pointer");
                gnd_assert(!src, -1, "invalid null pointer");

                { // ---> operation
                        int ret;
                        gnd::conf::file_stream fs;
                        // convert configuration declaration
                        if( (ret = proc_conf_set(&fs, src)) < 0 ) return ret;

                        return fs.write(f);
                } // <--- operation
        }


	} // namespace position_tracker
}; // namespace psm
// <--- function definition


#undef __OPTIMIZER_NEWTON__
#undef __OPTIMIZER_QMC__
#undef __OPTIMIZER_QMC2NEWTON__

#endif /* PSM_POSITION_TRACKER_CONF_HPP_ */