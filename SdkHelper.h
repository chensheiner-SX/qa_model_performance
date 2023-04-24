//
// Created by Tom Dayan on 03/05/2022.
//

#ifndef SDK_TESTER_SDKHELPER_H
#define SDK_TESTER_SDKHELPER_H

#include "Configuration.h"
#include <iostream>

using namespace sightx;

/**
 * Get Start Stream Detectors
 */
sdk::PreprocessorStartStreamConfiguration getDetectorConfiguration(sdk::StartStreamConfiguration& startConfiguration, size_t x) {
    //std::cout << "\n*-*-*-*-*-*-*-*-* Preprocessor *-*-*-*-*-*-*-*" << std::endl;
    return startConfiguration.getPreprocessor();
}

sdk::SeaMwirDetectorStartStreamConfiguration getDetectorConfiguration(sdk::StartStreamConfiguration& startConfiguration, int x) {
    //std::cout << "\n*-*-*-*-*-*-*-*-* Sea Mwir Detector *-*-*-*-*-*-*-*" << std::endl;
    return startConfiguration.getSeaMwirDetector();
}

sdk::SeaSwirDetectorStartStreamConfiguration getDetectorConfiguration(sdk::StartStreamConfiguration& startConfiguration, uint32_t x) {
    //std::cout << "\n*-*-*-*-*-*-*-*-* Sea Swir Detector *-*-*-*-*-*-*-*" << std::endl;
    return startConfiguration.getSeaSwirDetector();
}

sdk::GroundMwirDetectorStartStreamConfiguration getDetectorConfiguration(sdk::StartStreamConfiguration& startConfiguration, float x) {
    //std::cout << "\n*-*-*-*-*-*-*-*-* Ground Mwir Detector *-*-*-*-*-*-*-*" << std::endl;
    return startConfiguration.getGroundMwirDetector();
}

sdk::GroundRgbSwirDetectorStartStreamConfiguration getDetectorConfiguration(sdk::StartStreamConfiguration& startConfiguration, double x) {
    //std::cout << "\n*-*-*-*-*-*-*-*-* Ground Rgb and Swir Detector *-*-*-*-*-*-*-*" << std::endl;
    return startConfiguration.getGroundRgbSwirDetector();
}
//sdk::GroundRgbDetectorStartStreamConfiguration getDetectorConfiguration(sdk::StartStreamConfiguration& startConfiguration, double x) {
//    //std::cout << "\n*-*-*-*-*-*-*-*-* Ground Rgb and Swir Detector *-*-*-*-*-*-*-*" << std::endl;
//    return startConfiguration.getGroundRgbDetector();
//}
//sdk::GroundSwirDetectorStartStreamConfiguration getDetectorConfiguration(sdk::StartStreamConfiguration& startConfiguration, long x) {
//    //std::cout << "\n*-*-*-*-*-*-*-*-* Ground Rgb and Swir Detector *-*-*-*-*-*-*-*" << std::endl;
//    return startConfiguration.getGroundSwirDetector();
//}

/**
 * Get Update Stream Detectors
 */
sdk::SeaMwirDetectorUpdateStreamConfiguration getDetectorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, int x) {
   // std::cout << "\n*-*-*-*-*-*-*-*-* Sea Mwir Detector *-*-*-*-*-*-*-*" << std::endl;
    return stream->getConfiguration().getSeaMwirDetector();
}

sdk::SeaSwirDetectorUpdateStreamConfiguration getDetectorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, uint32_t x) {
    //std::cout << "\n*-*-*-*-*-*-*-*-* Sea Swir Detector *-*-*-*-*-*-*-*" << std::endl;
    return stream->getConfiguration().getSeaSwirDetector();
}

sdk::GroundMwirDetectorUpdateStreamConfiguration getDetectorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, float x) {
    //std::cout << "\n*-*-*-*-*-*-*-*-* Ground Mwir Detector *-*-*-*-*-*-*-*" << std::endl;
    return stream->getConfiguration().getGroundMwirDetector();
}

sdk::GroundRgbSwirDetectorUpdateStreamConfiguration getDetectorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, double x) {
    //std::cout << "\n*-*-*-*-*-*-*-*-* Ground Rgb and Swir Detector *-*-*-*-*-*-*-*" << std::endl;
    return stream->getConfiguration().getGroundRgbSwirDetector();
}
//sdk::GroundRgbDetectorUpdateStreamConfiguration getDetectorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, double x) {
//    //std::cout << "\n*-*-*-*-*-*-*-*-* Ground Rgb and Swir Detector *-*-*-*-*-*-*-*" << std::endl;
//    return stream->getConfiguration().getGroundRgbDetector();
//}
//sdk::GroundSwirDetectorUpdateStreamConfiguration getDetectorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, long x) {
//    //std::cout << "\n*-*-*-*-*-*-*-*-* Ground Rgb and Swir Detector *-*-*-*-*-*-*-*" << std::endl;
//    return stream->getConfiguration().getGroundSwirDetector();
//}


/*
 * get flow tracker rate
 * */

//sdk::FramerateControllerStartStreamConfiguration getTrackerRateStartConfiguration(sdk::StartStreamConfiguration startStreamConfiguration, uint64_t x){
//    return startStreamConfiguration.getFramerateController();
//}
sdk::SeaMwirTrackerRateStartStreamConfiguration getTrackerRateStartConfiguration(sdk::StartStreamConfiguration& startConfiguration, int x){
    return startConfiguration.getSeaMwirTrackerRate();
}
sdk::SeaSwirTrackerRateStartStreamConfiguration getTrackerRateStartConfiguration(sdk::StartStreamConfiguration& startConfiguration, uint32_t x){
    return startConfiguration.getSeaSwirTrackerRate();
}
sdk::GroundMwirTrackerRateStartStreamConfiguration getTrackerRateStartConfiguration(sdk::StartStreamConfiguration& startConfiguration, float x){
    return startConfiguration.getGroundMwirTrackerRate();
}
sdk::GroundRgbSwirTrackerRateStartStreamConfiguration getTrackerRateStartConfiguration(sdk::StartStreamConfiguration& startConfiguration, double x){
    return startConfiguration.getGroundRgbSwirTrackerRate();
}
//sdk::GroundRgbTrackerRateStartStreamConfiguration getTrackerRateStartConfiguration(sdk::StartStreamConfiguration& startConfiguration, double x){
//    return startConfiguration.getGroundRgbTrackerRate();
//}
//sdk::GroundSwirTrackerRateStartStreamConfiguration getTrackerRateStartConfiguration(sdk::StartStreamConfiguration& startConfiguration, long x){
//    return startConfiguration.getGroundSwirTrackerRate();
//}


/*
 * get flow update tracker rate
 * */
sdk::SeaMwirTrackerRateUpdateStreamConfiguration getTrackerRateUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, int x){
    return stream->getConfiguration().getSeaMwirTrackerRate();
}
sdk::SeaSwirTrackerRateUpdateStreamConfiguration getTrackerRateUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, uint32_t x){
    return stream->getConfiguration().getSeaSwirTrackerRate();
}
sdk::GroundMwirTrackerRateUpdateStreamConfiguration getTrackerRateUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, float x){
    return stream->getConfiguration().getGroundMwirTrackerRate();
}
sdk::GroundRgbSwirTrackerRateUpdateStreamConfiguration getTrackerRateUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, double x){
    return stream->getConfiguration().getGroundRgbSwirTrackerRate();
}
//sdk::GroundRgbTrackerRateUpdateStreamConfiguration getTrackerRateUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, double x){
//    return stream->getConfiguration().getGroundRgbTrackerRate();
//}
//sdk::GroundSwirTrackerRateUpdateStreamConfiguration getTrackerRateUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, long x){
//    return stream->getConfiguration().getGroundSwirTrackerRate();
//}


/*
 * get flow tracker
 * */
sdk::SeaMwirTrackerStartStreamConfiguration getTrackerStartConfiguration(sdk::StartStreamConfiguration& startConfiguration, int x){
    return startConfiguration.getSeaMwirTracker();
}
sdk::SeaSwirTrackerStartStreamConfiguration getTrackerStartConfiguration(sdk::StartStreamConfiguration& startConfiguration, uint32_t x){
    return startConfiguration.getSeaSwirTracker();
}
sdk::GroundMwirTrackerStartStreamConfiguration getTrackerStartConfiguration(sdk::StartStreamConfiguration& startConfiguration, float x){
    return startConfiguration.getGroundMwirTracker();
}
sdk::GroundRgbSwirTrackerStartStreamConfiguration getTrackerStartConfiguration(sdk::StartStreamConfiguration& startConfiguration, double x){
    return startConfiguration.getGroundRgbSwirTracker();
}

//sdk::GroundRgbTrackerStartStreamConfiguration getTrackerStartConfiguration(sdk::StartStreamConfiguration& startConfiguration, double x){
//    return startConfiguration.getGroundRgbTracker();
//}
//sdk::GroundSwirTrackerStartStreamConfiguration getTrackerStartConfiguration(sdk::StartStreamConfiguration& startConfiguration, long x){
//    return startConfiguration.getGroundSwirTracker();
//}
/*
 * get flow update tracker
 * */
sdk::SeaMwirTrackerUpdateStreamConfiguration getTrackerUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, int x){
    return stream->getConfiguration().getSeaMwirTracker();
}
sdk::SeaSwirTrackerUpdateStreamConfiguration getTrackerUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, uint32_t x){
    return stream->getConfiguration().getSeaSwirTracker();
}
sdk::GroundMwirTrackerUpdateStreamConfiguration getTrackerUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, float x){
    return stream->getConfiguration().getGroundMwirTracker();
}
sdk::GroundRgbSwirTrackerUpdateStreamConfiguration getTrackerUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, double x){
    return stream->getConfiguration().getGroundRgbSwirTracker();
}

//sdk::GroundRgbTrackerUpdateStreamConfiguration getTrackerUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, double x){
//    return stream->getConfiguration().getGroundRgbTracker();
//}
//sdk::GroundSwirTrackerUpdateStreamConfiguration getTrackerUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, long x){
//    return stream->getConfiguration().getGroundSwirTracker();
//}

/**
 * Get Start Stream Postprocessors
 */
// sdk::SeaMwirPostprocessorStartStreamConfiguration getPostprocessorConfiguration(sdk::StartStreamConfiguration& startConfiguration, int x) {
//     std::cout << "\n*-*-*-*-*-*-*-*-* Sea Mwir Detector *-*-*-*-*-*-*-*" << std::endl;
//     return startConfiguration.getSeaMwirPostprocessor();
// }

// sdk::SeaSwirPostprocessorStartStreamConfiguration getPostprocessorConfiguration(sdk::StartStreamConfiguration& startConfiguration, uint32_t x) {
//     std::cout << "\n*-*-*-*-*-*-*-*-* Sea Swir Detector *-*-*-*-*-*-*-*" << std::endl;
//     return startConfiguration.getSeaSwirPostprocessor();
// }

// sdk::GroundMwirPostprocessorStartStreamConfiguration getPostprocessorConfiguration(sdk::StartStreamConfiguration& startConfiguration, float x) {
//     std::cout << "\n*-*-*-*-*-*-*-*-* Ground Mwir Detector *-*-*-*-*-*-*-*" << std::endl;
//     return startConfiguration.getGroundMwirPostprocessor();
// }

// sdk::GroundRgbPostprocessorStartStreamConfiguration getPostprocessorConfiguration(sdk::StartStreamConfiguration& startConfiguration, double x) {
//     std::cout << "\n*-*-*-*-*-*-*-*-* Ground Rgb and Swir Detector *-*-*-*-*-*-*-*" << std::endl;
//     return startConfiguration.getGroundRgbPostprocessor();
// }

// /**
//  * Get Update Stream Postprocessors
//  */
// sdk::SeaMwirPostprocessorUpdateStreamConfiguration getPostprocessorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, int x) {
//     std::cout << "\n*-*-*-*-*-*-*-*-* Sea Mwir Detector *-*-*-*-*-*-*-*" << std::endl;
//     return stream->getConfiguration().getSeaMwirPostprocessor();
// }

// sdk::SeaSwirPostprocessorUpdateStreamConfiguration getPostprocessorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, uint32_t x) {
//     std::cout << "\n*-*-*-*-*-*-*-*-* Sea Swir Detector *-*-*-*-*-*-*-*" << std::endl;
//     return stream->getConfiguration().getSeaSwirPostprocessor();
// }

// sdk::GroundMwirPostprocessorUpdateStreamConfiguration getPostprocessorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, float x) {
//     std::cout << "\n*-*-*-*-*-*-*-*-* Ground Mwir Detector *-*-*-*-*-*-*-*" << std::endl;
//     return stream->getConfiguration().getGroundMwirPostprocessor();
// }

// sdk::GroundRgbPostprocessorUpdateStreamConfiguration getPostprocessorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, double x) {
//     std::cout << "\n*-*-*-*-*-*-*-*-* Ground Rgb and Swir Detector *-*-*-*-*-*-*-*" << std::endl;
//     return stream->getConfiguration().getGroundRgbPostprocessor();
// }
#endif //SDK_TESTER_SDKHELPER_H
