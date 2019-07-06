# a file copy build job to copy asset files into the project deployment directory
#
# also see fips-files/generators/filecopy.py
macro(sokol_copy_files yml_file)
    fips_generate(FROM ${yml_file}
        TYPE copy
        OUT_OF_SOURCE
        ARGS "{ deploy_dir: \"${FIPS_PROJECT_DEPLOY_DIR}\" }")
endmacro()
