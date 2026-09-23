#include "avemotion/reference/ReferenceRuntime.hpp"
#include "avemotion/reference/UpstreamInfo.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

int main() {
    const auto upstream = avemotion::reference::selectedUpstream();
    std::cout << "AveMotion reference probe\n"
              << "variant:    " << upstream.variant << '\n'
              << "repository: " << upstream.repository << '\n'
              << "commit:     " << upstream.commit << '\n'
              << "rlottie:    "
              << (avemotion::reference::ReferenceRuntime::compiledWithRlottie()
                      ? "available"
                      : "unavailable")
              << '\n';

    constexpr std::string_view minimal = R"json({
      "v":"5.5.7","fr":60,"ip":0,"op":60,"w":64,"h":64,"nm":"probe",
      "ddd":0,"assets":[],
      "layers":[{
        "ddd":0,"ind":1,"ty":4,"nm":"red square","sr":1,"ks":{
          "o":{"a":0,"k":100},"r":{"a":0,"k":0},
          "p":{"a":0,"k":[32,32,0]},"a":{"a":0,"k":[0,0,0]},
          "s":{"a":0,"k":[100,100,100]}},
        "shapes":[
          {"ty":"rc","d":1,"s":{"a":0,"k":[30,30]},"p":{"a":0,"k":[0,0]},"r":{"a":0,"k":0},"nm":"rect"},
          {"ty":"fl","c":{"a":0,"k":[1,0,0,1]},"o":{"a":0,"k":100},"r":1,"nm":"fill"}],
        "ip":0,"op":60,"st":0,"bm":0
      }]
    })json";

    avemotion::reference::ReferenceRuntime runtime;
    auto loaded = runtime.loadJson(minimal, "probe", false);
    if (!loaded) {
        std::cerr << "load failed: " << loaded.error << '\n';
        return EXIT_FAILURE;
    }

    const auto& metadata = loaded.animation->metadata();
    const auto frame = loaded.animation->renderFrame(0U, 64U, 64U);
    std::cout << "metadata:   " << metadata.width << 'x' << metadata.height
              << " @ " << metadata.frameRate << " fps, "
              << metadata.totalFrames << " frames\n"
              << "frame hash: " << avemotion::reference::formatHash(frame.fnv1a64)
              << " alpha-pixels=" << frame.nonTransparentPixels << '\n';
    return frame.nonTransparentPixels == 0U ? EXIT_FAILURE : EXIT_SUCCESS;
}
