- get the helmet asset from here: https://github.com/KhronosGroup/glTF-Sample-Models
- compile the basisu tool from https://github.com/BinomialLLC/basis_universal
    > mkdir build && cd build && cmake .. && cmake --build .
- install imagemagick
- resize and swap R<=>B channel of the Default_metalRoughness.jpg texture:
    > convert Default_metalRoughness.jpg -resize 1024x1024 -separate -swap 0,2 -combine Default_metalRoughness.jpg
- convert the textures like this:
    > ./basisu -mipmap -mip_srgb Default_albedo.jpg
    > ./basisu -mipmap -mip_srgb Default_emissive.jpg
    > ./basisu -mipmap -linear -mip_linear Default_AO.jpg
    > ./basisu -mipmap -normal_map -separate_rg_to_color_alpha Default_normal.jpg
    > ./basisu -mipmap -mip_linear -linear -separate_rg_to_color_alpha Default_metalRoughness.jpg


