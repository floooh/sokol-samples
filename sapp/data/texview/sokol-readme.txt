- compile the basisu tool from https://github.com/BinomialLLC/basis_universal
    > mkdir build && cd build && cmake .. && cmake --build .
- get the test images from the same repo and then:
    > basisu *.png -basis -mipmap -mip_slow -mip_clamp
