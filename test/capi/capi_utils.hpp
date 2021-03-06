/* Copyright © 2017 Apple Inc. All rights reserved.
 *
 * Use of this source code is governed by a BSD-3-clause license that can
 * be found in the LICENSE.txt file or at https://opensource.org/licenses/BSD-3-Clause
 */
#ifndef CAPI_TEST_UTILS
#define CAPI_TEST_UTILS

#include <capi/TuriCreate.h>
#include <capi/impl/capi_wrapper_structs.hpp>
#include <unity/lib/gl_sframe.hpp>
#include <unity/lib/gl_sarray.hpp>
#include <capi/impl/capi_wrapper_structs.hpp>
#include <flexible_type/flexible_type.hpp>
#include <cmath>
#include <vector>
#include <iostream>

#define CAPI_CHECK_ERROR(error)                                     \
  do {                                                              \
    if (error != nullptr) {                                         \
      std::ostringstream ss;                                        \
      ss << "C-API ERROR: " << __FILE__ << ": " << __LINE__ << ": " \
         << error->value;                                           \
      std::cout << ss.str() << std::endl;                           \
      throw std::runtime_error(ss.str());                           \
    }                                                               \
  } while (false)




static tc_flex_list* make_flex_list_double(const std::vector<double>& v) {

  tc_error* error = NULL;

  tc_flex_list* fl = tc_flex_list_create(&error);

  CAPI_CHECK_ERROR(error);

  size_t pos = 0;

  for(double t : v) {
    tc_flexible_type* ft = tc_ft_create_from_double(t, &error);

    CAPI_CHECK_ERROR(error);

    size_t idx = tc_flex_list_add_element(fl, ft, &error);

    CAPI_CHECK_ERROR(error);

    TS_ASSERT_EQUALS(idx, pos);
    ++pos;

    size_t actual_size = tc_flex_list_size(fl);

    TS_ASSERT_EQUALS(pos, actual_size);

    tc_release(ft);
  }

  // Go through and make sure things are equal
  for(size_t i = 0; i < v.size(); ++i) {
    tc_flexible_type* ft = tc_flex_list_extract_element(fl, i, &error);

    CAPI_CHECK_ERROR(error);

    TS_ASSERT(tc_ft_is_double(ft) != 0);

    double val = tc_ft_double(ft, &error);
    CAPI_CHECK_ERROR(error);

    TS_ASSERT(v[i] == val);

    tc_release(ft);
  }

  return fl;
}

static tc_flex_list* make_flex_list_string(const std::vector<std::string>& v) {

  tc_error* error = NULL;

  tc_flex_list* fl = tc_flex_list_create(&error);

  CAPI_CHECK_ERROR(error);

  size_t pos = 0;

  for(std::string t : v) {
    tc_flexible_type* ft = tc_ft_create_from_cstring(t.c_str(), &error);

    CAPI_CHECK_ERROR(error);

    size_t idx = tc_flex_list_add_element(fl, ft, &error);

    CAPI_CHECK_ERROR(error);

    TS_ASSERT_EQUALS(idx, pos);
    ++pos;

    size_t actual_size = tc_flex_list_size(fl);

    TS_ASSERT_EQUALS(pos, actual_size);

    tc_release(ft);
  }

  return fl;
}

static tc_sarray* make_sarray_double(const std::vector<double>& v) {

  tc_error* error = NULL;

  tc_flex_list* fl = make_flex_list_double(v);

  tc_sarray* sa = tc_sarray_create_from_list(fl, &error);

  CAPI_CHECK_ERROR(error);

  {
    // Make sure it gets out what we want it to.
    for(size_t i = 0; i < v.size(); ++i) {
      tc_flexible_type* ft = tc_sarray_extract_element(sa, i, &error);
      CAPI_CHECK_ERROR(error);

      TS_ASSERT(tc_ft_is_double(ft) != 0);

      double val = tc_ft_double(ft, &error);
      CAPI_CHECK_ERROR(error);

      TS_ASSERT(v[i] == val);

      tc_release(ft);
    }
  }

  return sa;
}

static tc_sframe* make_sframe_double(const std::vector<std::pair<std::string, std::vector<double> > >& data ) {

    tc_error* error = NULL;

    tc_sframe* sf = tc_sframe_create_empty(&error);

    CAPI_CHECK_ERROR(error);

    for(auto p : data) {

      tc_sarray* sa = make_sarray_double(p.second);

      tc_sframe_add_column(sf, p.first.c_str(), sa, &error);

      CAPI_CHECK_ERROR(error);

      tc_release(sa);
    }

    // Check everything
    for(auto p : data) {
      // Make sure it gets out what we want it to.
      tc_sarray* sa = tc_sframe_extract_column_by_name(sf, p.first.c_str(), &error);

      CAPI_CHECK_ERROR(error);

      tc_sarray* ref_sa = make_sarray_double(p.second);

      TS_ASSERT(tc_sarray_equals(sa, ref_sa, &error));

      CAPI_CHECK_ERROR(error);

      tc_release(sa);
    }

    return sf;
  }

static bool check_equality_gl_sframe(
  const turi::gl_sframe& sf_gl, const turi::gl_sframe& ref_gl) {

  size_t num_columns_sf = sf_gl.num_columns();
  size_t num_columns_ref = ref_gl.num_columns();

  if (num_columns_sf == num_columns_ref) {
    std::vector<std::string> column_names_sf = sf_gl.column_names();
    std::vector<std::string> column_names_ref = ref_gl.column_names();
    TS_ASSERT(column_names_sf == column_names_ref);
    for (size_t column_index=0; column_index < num_columns_sf; column_index++) {
      // go through all columns and check for sarray equality one by one

      turi::gl_sarray column_sf = sf_gl.select_column(
        column_names_sf[column_index]);
      turi::gl_sarray column_ref = ref_gl.select_column(
        column_names_ref[column_index]);

      if (column_sf.dtype() != column_ref.dtype()) return false;

      for (size_t i = 0; i < column_sf.size(); i++) {
        if (column_sf[i].get_type() == turi::flex_type_enum::UNDEFINED
          || column_ref[i].get_type() == turi::flex_type_enum::UNDEFINED) {
          if (column_sf[i] != column_ref[i]) return false;
          else continue;
        }
        if (column_sf.dtype() == turi::flex_type_enum::FLOAT) {
          if ((std::isnan(column_sf[i].get<turi::flex_float>()))
            && (std::isnan(column_ref[i].get<turi::flex_float>()))) {
            continue;
          }
          if ((std::isinf(column_sf[i].get<turi::flex_float>()))
            && (std::isinf(column_ref[i].get<turi::flex_float>()))) {
            // check for both positive or both negative
            if ((column_sf[i] > 0 && column_ref[i] < 0)
              || (column_sf[i] < 0 && column_ref[i] > 0)) {
              // their signs are different so not equal
              return false;
            } else continue;
          }
        }
        if (column_sf[i] != column_ref[i]) return false;
      }
      return true;
    }
  }
  return false;
}

#endif
