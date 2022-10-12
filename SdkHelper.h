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
    //std::cout << "*-*-*-*-*-*-*-* Preprocessor *-*-*-*-*-*-*-*" << std::endl;
    return startConfiguration.getPreprocessor();
}

sdk::SeaMwirDetectorStartStreamConfiguration getDetectorConfiguration(sdk::StartStreamConfiguration& startConfiguration, int x) {
    //std::cout << "*-*-*-*-*-*-*-* Sea Mwir Detector *-*-*-*-*-*-*-*" << std::endl;
    return startConfiguration.getSeaMwirDetector();
}

sdk::SeaSwirDetectorStartStreamConfiguration getDetectorConfiguration(sdk::StartStreamConfiguration& startConfiguration, uint32_t x) {
    //std::cout << "*-*-*-*-*-*-*-* Sea Swir Detector *-*-*-*-*-*-*-*" << std::endl;
    return startConfiguration.getSeaSwirDetector();
}

sdk::GroundMwirDetectorStartStreamConfiguration getDetectorConfiguration(sdk::StartStreamConfiguration& startConfiguration, float x) {
    //std::cout << "*-*-*-*-*-*-*-* Ground Mwir Detector *-*-*-*-*-*-*-*" << std::endl;
    return startConfiguration.getGroundMwirDetector();
}

sdk::GroundRgbSwirDetectorStartStreamConfiguration getDetectorConfiguration(sdk::StartStreamConfiguration& startConfiguration, double x) {
    //std::cout << "*-*-*-*-*-*-*-* Ground Rgb and Swir Detector *-*-*-*-*-*-*-*" << std::endl;
    return startConfiguration.getGroundRgbSwirDetector();
}

/**
 * Get Update Stream Detectors
 */
sdk::SeaMwirDetectorUpdateStreamConfiguration getDetectorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, int x) {
   // std::cout << "*-*-*-*-*-*-*-* Sea Mwir Detector *-*-*-*-*-*-*-*" << std::endl;
    return stream->getConfiguration().getSeaMwirDetector();
}

sdk::SeaSwirDetectorUpdateStreamConfiguration getDetectorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, uint32_t x) {
    //std::cout << "*-*-*-*-*-*-*-* Sea Swir Detector *-*-*-*-*-*-*-*" << std::endl;
    return stream->getConfiguration().getSeaSwirDetector();
}

sdk::GroundMwirDetectorUpdateStreamConfiguration getDetectorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, float x) {
    //std::cout << "*-*-*-*-*-*-*-* Ground Mwir Detector *-*-*-*-*-*-*-*" << std::endl;
    return stream->getConfiguration().getGroundMwirDetector();
}

sdk::GroundRgbSwirDetectorUpdateStreamConfiguration getDetectorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, double x) {
    //std::cout << "*-*-*-*-*-*-*-* Ground Rgb and Swir Detector *-*-*-*-*-*-*-*" << std::endl;
    return stream->getConfiguration().getGroundRgbSwirDetector();
}
/*
 * get flow tracker rate
 * */

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

/*
 * get flow update tracker
 * */


/**
 * Get Start Stream Postprocessors
 */
// sdk::SeaMwirPostprocessorStartStreamConfiguration getPostprocessorConfiguration(sdk::StartStreamConfiguration& startConfiguration, int x) {
//     std::cout << "*-*-*-*-*-*-*-* Sea Mwir Detector *-*-*-*-*-*-*-*" << std::endl;
//     return startConfiguration.getSeaMwirPostprocessor();
// }

// sdk::SeaSwirPostprocessorStartStreamConfiguration getPostprocessorConfiguration(sdk::StartStreamConfiguration& startConfiguration, uint32_t x) {
//     std::cout << "*-*-*-*-*-*-*-* Sea Swir Detector *-*-*-*-*-*-*-*" << std::endl;
//     return startConfiguration.getSeaSwirPostprocessor();
// }

// sdk::GroundMwirPostprocessorStartStreamConfiguration getPostprocessorConfiguration(sdk::StartStreamConfiguration& startConfiguration, float x) {
//     std::cout << "*-*-*-*-*-*-*-* Ground Mwir Detector *-*-*-*-*-*-*-*" << std::endl;
//     return startConfiguration.getGroundMwirPostprocessor();
// }

// sdk::GroundRgbSwirPostprocessorStartStreamConfiguration getPostprocessorConfiguration(sdk::StartStreamConfiguration& startConfiguration, double x) {
//     std::cout << "*-*-*-*-*-*-*-* Ground Rgb and Swir Detector *-*-*-*-*-*-*-*" << std::endl;
//     return startConfiguration.getGroundRgbSwirPostprocessor();
// }

// /**
//  * Get Update Stream Postprocessors
//  */
// sdk::SeaMwirPostprocessorUpdateStreamConfiguration getPostprocessorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, int x) {
//     std::cout << "*-*-*-*-*-*-*-* Sea Mwir Detector *-*-*-*-*-*-*-*" << std::endl;
//     return stream->getConfiguration().getSeaMwirPostprocessor();
// }

// sdk::SeaSwirPostprocessorUpdateStreamConfiguration getPostprocessorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, uint32_t x) {
//     std::cout << "*-*-*-*-*-*-*-* Sea Swir Detector *-*-*-*-*-*-*-*" << std::endl;
//     return stream->getConfiguration().getSeaSwirPostprocessor();
// }

// sdk::GroundMwirPostprocessorUpdateStreamConfiguration getPostprocessorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, float x) {
//     std::cout << "*-*-*-*-*-*-*-* Ground Mwir Detector *-*-*-*-*-*-*-*" << std::endl;
//     return stream->getConfiguration().getGroundMwirPostprocessor();
// }

// sdk::GroundRgbSwirPostprocessorUpdateStreamConfiguration getPostprocessorUpdateConfiguration(std::shared_ptr<sdk::Stream>& stream, double x) {
//     std::cout << "*-*-*-*-*-*-*-* Ground Rgb and Swir Detector *-*-*-*-*-*-*-*" << std::endl;
//     return stream->getConfiguration().getGroundRgbSwirPostprocessor();
// }
#endif //SDK_TESTER_SDKHELPER_H
