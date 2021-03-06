/*
 * Copyright © 2019 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "nir.h"
#include "util/bitset.h"

static void
set_type(unsigned idx, nir_alu_type type, BITSET_WORD *float_types,
         BITSET_WORD *int_types, bool *progress)
{
   switch (nir_alu_type_get_base_type(type)) {
   case nir_type_bool:
   case nir_type_int:
   case nir_type_uint:
      if (int_types && !BITSET_TEST(int_types, idx)) {
         *progress = true;
         BITSET_SET(int_types, idx);
      }
      break;

   case nir_type_float:
      if (float_types && !BITSET_TEST(float_types, idx)) {
         *progress = true;
         BITSET_SET(float_types, idx);
      }
      break;

   default:
      unreachable("Invalid base nir_alu_type");
   }
}

static void
copy_types(unsigned a, unsigned b, BITSET_WORD *float_types,
           BITSET_WORD *int_types, bool *progress)
{
   /* If the bits do not agree then one of them is set but not both.  Flag
    * progress and set both bits.
    */
   if (float_types && (BITSET_TEST(float_types, a) !=
                       BITSET_TEST(float_types, b))) {
      *progress = true;
      BITSET_SET(float_types, a);
      BITSET_SET(float_types, b);
   }

   if (int_types && (BITSET_TEST(int_types, a) !=
                     BITSET_TEST(int_types, b))) {
      *progress = true;
      BITSET_SET(int_types, a);
      BITSET_SET(int_types, b);
   }
}

/** Gather up ALU types for SSA values
 *
 * This pass attempts to determine, for each SSA value, the type of data (int
 * or float) that will be stored in it.  The pass is greedy in the sense that
 * it just assigns intness or floatness to types without any attempt to sort
 * out the interesting cases where a given type may be both.
 *
 * The output of the pass is a pair of bitsets which has the intness or
 * floatness of each SSA value recorded by index.  It is the responsibility of
 * the caller to index the SSA defs using nir_index_ssa_defs and allocate the
 * bitsets.  Either bitset is allowed to be NULL in which case no data is
 * recorded for that type.
 */
void
nir_gather_ssa_types(nir_function_impl *impl,
                     BITSET_WORD *float_types,
                     BITSET_WORD *int_types)
{
   bool progress;
   do {
      progress = false;

      nir_foreach_block(block, impl) {
         nir_foreach_instr(instr, block) {
            switch (instr->type) {
            case nir_instr_type_alu: {
               nir_alu_instr *alu = nir_instr_as_alu(instr);
               assert(alu->dest.dest.is_ssa);
               const nir_op_info *info = &nir_op_infos[alu->op];
               switch (alu->op) {
               case nir_op_imov:
               case nir_op_fmov:
               case nir_op_vec2:
               case nir_op_vec3:
               case nir_op_vec4:
                  for (unsigned i = 0; i < info->num_inputs; i++) {
                     assert(alu->src[i].src.is_ssa);
                     copy_types(alu->src[i].src.ssa->index,
                                alu->dest.dest.ssa.index,
                                float_types, int_types, &progress);
                  }
                  break;

               case nir_op_bcsel:
               case nir_op_b32csel:
                  set_type(alu->src[0].src.ssa->index, nir_type_bool,
                           float_types, int_types, &progress);
                  copy_types(alu->src[1].src.ssa->index,
                             alu->dest.dest.ssa.index,
                             float_types, int_types, &progress);
                  copy_types(alu->src[2].src.ssa->index,
                             alu->dest.dest.ssa.index,
                             float_types, int_types, &progress);
                  break;

               default:
                  for (unsigned i = 0; i < info->num_inputs; i++) {
                     assert(alu->src[i].src.is_ssa);
                     set_type(alu->src[i].src.ssa->index, info->input_types[i],
                              float_types, int_types, &progress);
                  }
                  set_type(alu->dest.dest.ssa.index, info->output_type,
                           float_types, int_types, &progress);
               }
               break;
            }

            case nir_instr_type_tex: {
               nir_tex_instr *tex = nir_instr_as_tex(instr);
               for (unsigned i = 0; i < tex->num_srcs; i++) {
                  assert(tex->src[i].src.is_ssa);
                  set_type(tex->src[i].src.ssa->index,
                           nir_tex_instr_src_type(tex, i),
                           float_types, int_types, &progress);
               }
               assert(tex->dest.is_ssa);
               set_type(tex->dest.ssa.index, tex->dest_type,
                        float_types, int_types, &progress);
               break;
            }

            case nir_instr_type_intrinsic: {
               nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
               /* We could go nuts here, but we'll just handle a few simple
                * cases and let everything else be untyped.
                */
               switch (intrin->intrinsic) {
               case nir_intrinsic_load_deref: {
                  nir_deref_instr *deref = nir_src_as_deref(intrin->src[0]);

                  assert(intrin->dest.is_ssa);
                  set_type(intrin->dest.ssa.index,
                           nir_get_nir_type_for_glsl_type(deref->type),
                           float_types, int_types, &progress);
                  break;
               }

               case nir_intrinsic_store_deref: {
                  nir_deref_instr *deref = nir_src_as_deref(intrin->src[0]);

                  assert(intrin->src[1].is_ssa);
                  set_type(intrin->src[1].ssa->index,
                           nir_get_nir_type_for_glsl_type(deref->type),
                           float_types, int_types, &progress);
                  break;
               }

               default: {
                  /* For the most part, we leave other intrinsics alone.  Most
                   * of them don't matter in OpenGL ES 2.0 drivers anyway.
                   * However, we should at least check if this is some sort of
                   * IO intrinsic and flag it's offset and index sources.
                   */
                  nir_src *offset_src = nir_get_io_offset_src(intrin);
                  if (offset_src) {
                     assert(offset_src->is_ssa);
                     set_type(offset_src->ssa->index, nir_type_int,
                              float_types, int_types, &progress);
                  }
                  break;
               }
               }
               break;
            }

            case nir_instr_type_phi: {
               nir_phi_instr *phi = nir_instr_as_phi(instr);
               assert(phi->dest.is_ssa);
               nir_foreach_phi_src(src, phi) {
                  assert(src->src.is_ssa);
                  copy_types(src->src.ssa->index, phi->dest.ssa.index,
                             float_types, int_types, &progress);
               }
               break;
            }

            default:
               break;
            }
         }
      }
   } while (progress);
}
