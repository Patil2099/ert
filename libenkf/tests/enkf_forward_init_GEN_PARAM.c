/*
   Copyright (C) 2013  Statoil ASA, Norway.

   The file 'enkf_forward_init_GEN_PARAM.c' is part of ERT - Ensemble based Reservoir Tool.

   ERT is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   ERT is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.

   See the GNU General Public License at <http://www.gnu.org/licenses/gpl.html>
   for more details.
*/
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include <ert/util/rng.h>
#include <ert/util/mzran.h>
#include <ert/util/test_util.h>
#include <ert/util/test_work_area.h>
#include <ert/util/util.h>
#include <ert/util/thread_pool.h>
#include <ert/util/arg_pack.h>

#include <ert/enkf/enkf_main.h>
#include <ert/enkf/run_arg.h>


void create_runpath(enkf_main_type * enkf_main, int iter ) {
  const int ens_size         = enkf_main_get_ensemble_size( enkf_main );
  bool_vector_type * iactive = bool_vector_alloc(0,false);

  bool_vector_iset( iactive , ens_size - 1 , true );
  enkf_main_create_run_path(enkf_main , iactive , iter);
  bool_vector_free(iactive);
}



int main(int argc , char ** argv) {
  enkf_main_install_SIGNALS();
  rng_type * rng = rng_alloc( MZRAN , INIT_DEFAULT );
  const char * root_path = argv[1];
  const char * config_file = argv[2];
  const char * forward_init_string = argv[3];
  test_work_area_type * work_area = test_work_area_alloc(config_file );
  test_work_area_copy_directory_content( work_area , root_path );
  {
    bool forward_init;
    bool strict = true;
    enkf_main_type * enkf_main;

    test_assert_true( util_sscanf_bool( forward_init_string , &forward_init));

    util_clear_directory( "Storage" , true , true );
    res_config_type * res_config = res_config_alloc_load(config_file);
    enkf_main = enkf_main_alloc(res_config, strict, true);
    {
      const enkf_config_node_type * config_node = ensemble_config_get_node( enkf_main_get_ensemble_config( enkf_main ) , "PARAM" );
      enkf_node_type * gen_param_node = enkf_node_alloc( config_node );
      {
        const enkf_config_node_type * gen_param_config_node = enkf_node_get_config( gen_param_node );
        char * init_file1 = enkf_config_node_alloc_initfile( gen_param_config_node , NULL , 0);
        char * init_file2 = enkf_config_node_alloc_initfile( gen_param_config_node , "/tmp", 0);

        test_assert_bool_equal( enkf_config_node_use_forward_init( gen_param_config_node ) , forward_init );
        test_assert_string_equal( init_file1 , "PARAM_INIT");
        test_assert_string_equal( init_file2 , "/tmp/PARAM_INIT");

        free( init_file1 );
        free( init_file2 );
      }

      test_assert_bool_equal( enkf_node_use_forward_init( gen_param_node ) , forward_init );
      if (forward_init)
        test_assert_bool_not_equal( enkf_node_initialize( gen_param_node , 0 , rng), forward_init);
      // else hard_failure()
      enkf_node_free( gen_param_node );
    }
    test_assert_bool_equal( forward_init, ensemble_config_have_forward_init( enkf_main_get_ensemble_config( enkf_main )));

    if (forward_init) {
      enkf_state_type * state   = enkf_main_iget_state( enkf_main , 0 );
      const enkf_config_node_type * config_node = ensemble_config_get_node( enkf_main_get_ensemble_config( enkf_main ) , "PARAM" );
      enkf_node_type * gen_param_node = enkf_node_alloc( config_node );
      enkf_fs_type * fs = enkf_main_get_fs( enkf_main );
      run_arg_type * run_arg = run_arg_alloc_ENSEMBLE_EXPERIMENT( fs , 0 , 0 , "simulations/run0");

      node_id_type node_id = {.report_step = 0 ,
                              .iens = 0};

      create_runpath( enkf_main, 0 );
      test_assert_true( util_is_directory( "simulations/run0" ));

      test_assert_false( enkf_node_has_data( gen_param_node , fs, node_id ));
      util_unlink_existing( "simulations/run0/PARAM_INIT" );

      {
        FILE * stream = util_fopen("simulations/run0/PARAM_INIT" , "w");
        fprintf(stream , "0\n1\n2\n3\n" );
        fclose( stream );
      }

      {
        int error;
        stringlist_type * msg_list = stringlist_alloc_new();

        test_assert_true( enkf_node_forward_init( gen_param_node , "simulations/run0" , 0 ));

        error = enkf_state_forward_init( state , run_arg );
        test_assert_int_equal(0, error);
        {
          enkf_fs_type * fs = enkf_main_get_fs( enkf_main );
          state_map_type * state_map = enkf_fs_get_state_map(fs);
          state_map_iset(state_map , 0 , STATE_INITIALIZED);
        }
        error = enkf_state_load_from_forward_model( state , run_arg , msg_list );

        stringlist_free( msg_list );
        test_assert_int_equal(0, error);

        {
          double value;
          test_assert_true( enkf_node_user_get( gen_param_node , fs , "0" , node_id , &value));
          test_assert_double_equal( 0 , value);

          test_assert_true( enkf_node_user_get( gen_param_node , fs , "1" , node_id , &value));
          test_assert_double_equal( 1 , value);

          test_assert_true( enkf_node_user_get( gen_param_node , fs , "2" , node_id , &value));
          test_assert_double_equal( 2 , value);
        }
      }
      util_clear_directory( "simulations" , true , true );
      create_runpath( enkf_main, 0 );
      test_assert_true( util_is_directory( "simulations/run0" ));
      test_assert_true( util_is_file( "simulations/run0/PARAM.INC" ));
      {
        FILE * stream = util_fopen("simulations/run0/PARAM.INC" , "r");
        double v0,v1,v2,v3;
        fscanf(stream , "%lg %lg %lg %lg" , &v0,&v1,&v2,&v3);
        fclose( stream );
        test_assert_double_equal( 0 , v0);
        test_assert_double_equal( 1 , v1);
        test_assert_double_equal( 2 , v2);
        test_assert_double_equal( 3 , v3);
      }
      util_clear_directory( "simulations" , true , true );
      run_arg_free( run_arg );
      enkf_node_free( gen_param_node );
    }
    enkf_main_free( enkf_main );
    res_config_free(res_config);
  }
  test_work_area_free( work_area );
  rng_free( rng );
}

