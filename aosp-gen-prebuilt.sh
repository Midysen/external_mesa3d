mkdir -p prebuilt-intermediates/{glsl,ir3,main,nir,spirv,cle,isl,perf,genxml,compiler,iris,util,vulkan,xmlpool,v3d}

python src/compiler/glsl/ir_expression_operation.py strings > prebuilt-intermediates/glsl/ir_expression_operation_strings.h
python src/compiler/glsl/ir_expression_operation.py constant > prebuilt-intermediates/glsl/ir_expression_operation_constant.h
python src/compiler/glsl/ir_expression_operation.py enum > prebuilt-intermediates/glsl/ir_expression_operation.h

python src/mesa/main/format_pack.py  src/mesa/main/formats.csv  > prebuilt-intermediates/main/format_pack.c
python src/mesa/main/format_unpack.py  src/mesa/main/formats.csv  > prebuilt-intermediates/main/format_unpack.c
python src/mesa/main/format_fallback.py  src/mesa/main/formats.csv /dev/stdout  > prebuilt-intermediates/main/format_fallback.c

python src/compiler/nir/nir_builder_opcodes_h.py src/compiler/nir/nir_opcodes.py > prebuilt-intermediates/nir/nir_builder_opcodes.h
python src/compiler/nir/nir_constant_expressions.py src/compiler/nir/nir_opcodes.py > prebuilt-intermediates/nir/nir_constant_expressions.c
python src/compiler/nir/nir_opcodes_c.py src/compiler/nir/nir_opcodes.py > prebuilt-intermediates/nir/nir_opcodes.c
python src/compiler/nir/nir_opcodes_h.py src/compiler/nir/nir_opcodes.py > prebuilt-intermediates/nir/nir_opcodes.h
python src/compiler/nir/nir_opt_algebraic.py src/compiler/nir/nir_opt_algebraic.py > prebuilt-intermediates/nir/nir_opt_algebraic.c
python src/compiler/nir/nir_intrinsics_c.py --outdir prebuilt-intermediates/nir/ || ( prebuilt-intermediates/nir/nir_intrinsics.c; false)
python src/compiler/nir/nir_intrinsics_h.py --outdir prebuilt-intermediates/nir/ || ( prebuilt-intermediates/nir/nir_intrinsics.h; false)

python src/compiler/spirv/spirv_info_c.py src/compiler/spirv/spirv.core.grammar.json prebuilt-intermediates/spirv/spirv_info.c || ( prebuilt-intermediates/spirv/spirv_info.c; false)
python src/compiler/spirv/vtn_gather_types_c.py src/compiler/spirv/spirv.core.grammar.json prebuilt-intermediates/spirv/vtn_gather_types.c || ( prebuilt-intermediates/spirv/vtn_gather_types.c; false)

python src/util/format_srgb.py > prebuilt-intermediates/util/format_srgb.c
python src/util/format/u_format_table.py src/util/format/u_format.csv > prebuilt-intermediates/util/u_format_table.c

python src/intel/genxml/gen_zipped_file.py src/broadcom/cle/v3d_packet_v21.xml src/broadcom/cle/v3d_packet_v33.xml > prebuilt-intermediates/cle/v3d_xml.h

python src/broadcom/cle/gen_pack_header.py src/broadcom/cle/v3d_packet_v21.xml 21 > prebuilt-intermediates/cle/v3d_packet_v21_pack.h
python src/broadcom/cle/gen_pack_header.py src/broadcom/cle/v3d_packet_v33.xml 33 > prebuilt-intermediates/cle/v3d_packet_v33_pack.h
python src/broadcom/cle/gen_pack_header.py src/broadcom/cle/v3d_packet_v33.xml 41 > prebuilt-intermediates/cle/v3d_packet_v41_pack.h
python src/broadcom/cle/gen_pack_header.py src/broadcom/cle/v3d_packet_v33.xml 42 > prebuilt-intermediates/cle/v3d_packet_v42_pack.h

python src/util/merge_driinfo.py src/gallium/drivers/v3d/driinfo_v3d.h src/gallium/auxiliary/pipe-loader/driinfo_gallium.h > prebuilt-intermediates/v3d/v3d_driinfo.h


xgettext -L C --from-code utf-8 -o prebuilt-intermediates/xmlpool/xmlpool.pot src/util/xmlpool/t_options.h

for lang in de es nl fr sv ; do
        echo "for lang = $lang"
        if [ -f src/util/xmlpool/$lang.po ]; then
		msgmerge -o prebuilt-intermediates/xmlpool/$lang.po src/util/xmlpool/$lang.po prebuilt-intermediates/xmlpool/xmlpool.pot;
	else
		it -i prebuilt-intermediates/xmlpool/xmlpool.pot -o prebuilt-intermediates/xmlpool/$lang.po --locale=\$lang --no-translator;
		sed -i -e 's/charset=.*\\\\n/charset=UTF-8\\\\n/' prebuilt-intermediates/xmlpool/$lang.po;
	fi
        msgfmt -o prebuilt-intermediates/xmlpool/$lang.gmo prebuilt-intermediates/xmlpool/$lang.po
done

python src/util/xmlpool/gen_xmlpool.py --template src/util/xmlpool/t_options.h --output prebuilt-intermediates/xmlpool/options.h --localedir prebuilt-intermediates/xmlpool --languages de es nl fr sv

rm -f prebuilt-intermediates/xmlpool/*.po prebuilt-intermediates/xmlpool/*.gmo prebuilt-intermediates/xmlpool/xmlpool.pot
