#include "PipelineInterface.h"
#include "SdkHelper.h"
#include "/usr/local/Meerkat/include/Configuration.h"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <thread>
#include <cassert>
#include <map>
#include <typeindex>
//#include <random>

// Use (void) to silence unused warnings.
//#define assertm(exp, msg) assert(((void)msg, exp))

#include </usr/include/opencv4/opencv2/videoio.hpp>

#define BOOST_TEST_MODULE SDK testcases

#include <boost/core/ignore_unused.hpp>
#include <boost/test/included/unit_test.hpp>

using namespace sightx;

enum streamError {
    NONE,
    RotateAngleUnsupported,
    RoiIncorrectSize,
    PreviewError
};

enum rangeError {
    RotateAngle,
    ROI,
    Threshold,
    None
};

struct UserData {
    bool ServerState = false;
    bool StreamState = false;

    std::mutex Mutex;
    std::condition_variable Condition;
};

struct FrameDimensions {
    float Height;
    float Width;
};

struct Roi {
    float X;
    float Y;
    float Width;
    float Height;
};

std::array<std::string, 4> seaGroups = {"ship", "sailboat", "jetski", "motorboat"};
std::array<std::string, 4> groundGroups = {"light-vehicle", "person", "two-wheeled", "None"};
std::array<std::string, 4> currentDetectedGroups;
std::map <sdk::FlowSwitcherFlowId, std::string> printDetectorFlow = {
        {sdk::FlowSwitcherFlowId::SeaMwir, "*-*-*-*-*-*-*-* Sea Mwir Detector *-*-*-*-*-*-*-*"},
        {sdk::FlowSwitcherFlowId::SeaSwir, "*-*-*-*-*-*-*-* Sea Swir Detector *-*-*-*-*-*-*-*"},
        {sdk::FlowSwitcherFlowId::GroundRgbAndSwir, "*-*-*-*-*-*-*-* Ground RgbSwir Detector *-*-*-*-*-*-*-*"},
        {sdk::FlowSwitcherFlowId::GroundMwir, "*-*-*-*-*-*-*-* Ground Mwir Detector *-*-*-*-*-*-*-*"},
        {sdk::FlowSwitcherFlowId::Unspecified, "*-*-*-*-*-*-*-* Preprocessor *-*-*-*-*-*-*-*"}
};

std::map <sdk::FlowSwitcherFlowId, std::string> printTrackerFlow = {
        {sdk::FlowSwitcherFlowId::SeaMwir, "*-*-*-*-*-*-*-* Sea Mwir Tracker *-*-*-*-*-*-*-*"},
        {sdk::FlowSwitcherFlowId::SeaSwir, "*-*-*-*-*-*-*-* Sea Swir Tracker *-*-*-*-*-*-*-*"},
        {sdk::FlowSwitcherFlowId::GroundRgbAndSwir, "*-*-*-*-*-*-*-* Ground RgbSwir Tracker *-*-*-*-*-*-*-*"},
        {sdk::FlowSwitcherFlowId::GroundMwir, "*-*-*-*-*-*-*-* Ground Mwir Tracker *-*-*-*-*-*-*-*"},
};

UserData data;
streamError currentStreamError = streamError::NONE;
rangeError currentRangeError = rangeError::None; // TODO - check if needed
std::string currentErrorStr;
FrameDimensions currentFrameDimensions;
Roi currentRoi;
bool isRoiTest = false;
bool isArmTest = false;
std::shared_ptr <sdk::Pipeline> mainPipeline;
std::shared_ptr <sdk::Stream> currentStream;
std::map<std::string, bool> runningStreams;
cv::VideoCapture videoCapture;

std::string a_strVideoPath;
std::string a_strServerIP;
std::string sinkStr;
cv::Mat matFrame;
uint32_t numOfFrames = 500;
int errorCounter = 0;

std::string seaMwirVideo;
std::string seaSwirVideo;
std::string groundMwirVideo;
std::string groundRgbVideo;
std::string groundSwirVideo;

std::string rotateAngleNotInRange;
std::string rotateAngleUnsupported;
std::string ROINotInRange;
std::string ROIUnsupported;
std::string previewError;


/// \isInRoi
/// Checking if detected object is in defined ROI
/// \param objLocation detected object location
/// \return is object inside ROI
bool isInRoi(sdk::BoundingBox objLocation) {

    if (objLocation.X1 >= currentRoi.X && objLocation.Y1 >= currentRoi.Y &&
        objLocation.X2 <= (currentRoi.X + currentRoi.Width) && objLocation.Y2 <= (currentRoi.Y + currentRoi.Height))
        return true;


    std::cout << "Object: [" << objLocation.X1 << "," << objLocation.Y1 << "," << objLocation.X2 << ","
              << objLocation.Y2 << "]" << std::endl;
    std::cout << "ROI: [" << currentRoi.X << "," << currentRoi.Y << "," << currentRoi.X + currentRoi.Width
              << "," << currentRoi.Y + currentRoi.Height << std::endl;
    return false;


}
bool isArmed(const sdk::Attribute& attribute){
    return (attribute.Value == "yes");
}
void onMessage(const sdk::MessageLog &a_Log, void * /*a_pUserData*/) {
//    std::cout << static_cast<uint32_t>(a_Log.Level) << ": " << a_Log.Text.toString() << std::endl;
}

/// \onFrameResults
/// Function for checking the enginee's tracks results.
/// For each frame of the current stream, we are checking the followings: 1. Class threshold result 2. Detected object location compare to defined ROI
/// \param a_FrameResults Struct containing the track results

void onFrameResults(const sdk::FrameResults &a_FrameResults, void * /*a_pUserData*/) { //TODO - find a way to check detections and not tracks
//    std::cout << "\rFrame " << a_FrameResults.FrameId << " out of: " << numOfFrames <<
//                  " [" << a_FrameResults.StreamId.toString() << "]" << std::flush;
//    for(const sdk::Track& track : a_FrameResults.Tracks.toVector()){
//        std::cout << "Track id= " + std::to_string(track.TrackId) << std::endl;
//        std::cout << "Track class= " + track.DetectionData.Class.toString() << std::endl;
//        std::cout << "Track score= " + std::to_string(track.DetectionData.Score) << std::endl;
//        for(auto attribute : track.Attributes.toVector()){
//            std::cout << "Attribute name= " + attribute.Name.toString() << std::endl;
//            std::cout << "Attribute value= " + attribute.Value.toString() << std::endl;
//        }
//    }
    try {
        float threshold = 0;
        std::cout << "\rFrame " << a_FrameResults.FrameId << " out of: " << numOfFrames <<
                  " [" << a_FrameResults.StreamId.toString() << "]" << std::flush;
        if (a_FrameResults.StreamId == currentStream->getId().toString()) {
            for (const sdk::Track &track: a_FrameResults.Tracks.toVector()) {
                std::string trackClass = track.DetectionData.Class.toString();
                switch (currentStream->getConfiguration().getFlowSwitcher().getFlowId()) {
                    case sdk::FlowSwitcherFlowId::SeaMwir:
                        if (std::find(seaGroups.begin(), seaGroups.end(), trackClass) != seaGroups.end())
                            threshold = currentStream->getFullConfiguration().getSeaMwirDetector().getGroups(
                                    trackClass).getScoreThreshold();
                        else { //TODO - write "deeper" implementation for all "else" statements..?
                            std::cerr << "Found unsupported group!" << std::endl;
                        }
                        break;
                    case sdk::FlowSwitcherFlowId::SeaSwir:
                        if (std::find(seaGroups.begin(), seaGroups.end(), trackClass) != seaGroups.end())
                            threshold = currentStream->getFullConfiguration().getSeaSwirDetector().getGroups(
                                    trackClass).getScoreThreshold();
                        else {
                            std::cerr << "Found unsupported group!" << std::endl;
                        }
                        break;
                    case sdk::FlowSwitcherFlowId::GroundMwir:
                        if (std::find(groundGroups.begin(), groundGroups.end(), trackClass) != groundGroups.end())
                            threshold = currentStream->getFullConfiguration().getGroundMwirDetector().getGroups(
                                    trackClass).getScoreThreshold();
                        else {
                            std::cerr << "Found unsupported group!" << std::endl;
                        }
                        break;
                    case sdk::FlowSwitcherFlowId::GroundRgbAndSwir:
                        if (std::find(groundGroups.begin(), groundGroups.end(), trackClass) != groundGroups.end())
                            threshold = currentStream->getFullConfiguration().getGroundRgbSwirDetector().getGroups(
                                    trackClass).getScoreThreshold();
                        else{
                            std::cerr << "Found unsupported group!" << std::endl;
                        }
                        break;
                    default:
                        break;
                }
//                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                BOOST_TEST((track.DetectionData.Score >= threshold || track.DetectionData.Score == 0),
                           "Threshold for class " << trackClass << " [" << threshold << "] > Score ["
                                                  << track.DetectionData.Score <<
                                                  "] --> [" << a_FrameResults.StreamId << ", "
                                                  << currentStream->getId().toString() << "]");
                BOOST_TEST(isInRoi(track.DetectionData.Location));
                if(isArmTest){
                    //BOOST_TEST(isArmed(track.Attributes.data()), "Armed person not detected");
                    for(const auto attribute : track.Attributes.toVector()){
                        BOOST_TEST(isArmed(attribute), "Armed person not detected");
                    }
                }
            }
        }
    }
    catch (sdk::Exception &e) {
        BOOST_TEST(((std::string) e.what()).find("Deadline Exceeded") != std::string::npos,
                   "Caught unexpected error: " + (std::string) e.what());
    }

}

/// \streamErrorTest
/// Checking what stream error we get. The options are invalid rotate angle, unsupported ROI or Preview error.
/// For each streamError we caught, we checking the error string we get and decreasing the error counter
/// \param errorCaught The error message caught
/// \param streamId The stream Id

void streamErrorTest(const std::string& errorCaught, const std::string &streamId) {
    // If this is not "not registered" and "no data for..."
    if (errorCaught.find("not registered") == std::string::npos
        && errorCaught.find("No data read for 20 seconds, closing stream") == std::string::npos) {
        switch (currentStreamError) {
            case streamError::RotateAngleUnsupported:
                BOOST_TEST_MESSAGE("Rotate angle unsupported test --> " + streamId);
                BOOST_TEST(errorCaught.find(currentErrorStr) != std::string::npos);
                errorCounter = errorCounter > 0 ? errorCounter - 1 : 0;
                break;
            case streamError::RoiIncorrectSize:
                BOOST_TEST_MESSAGE("ROI incorrect size test --> " + streamId);
                BOOST_TEST(errorCaught.find(currentErrorStr) != std::string::npos); //TODO
//                std::cout << "errorCaught: " + errorCaught << std::endl;
//                std::cout << "errorStr: " + currentErrorStr << std::endl;
                errorCounter = errorCounter > 0 ? errorCounter - 1 : 0;
                break;
//            case streamError::ThresholdIncorrectSize:
//                BOOST_TEST_MESSAGE("Threshold incorrect size test --> ")

            case streamError::PreviewError:
                BOOST_TEST_MESSAGE("Preview error test --> " + streamId);
                BOOST_TEST(errorCaught.find(currentErrorStr) != std::string::npos);
                errorCounter = errorCounter > 0 ? errorCounter - 1 : 0;
                break;
            default:
                break;
        }
    }
    currentStreamError = streamError::NONE;
}
/// \outOfRangeErrorTest
/// Function verify each call that we get the relevant out-of-range error string.
/// We decreasing the error counter every call
/// \param errorMsg The error message caught

void outOfRangeErrorTest(const std::string &errorMsg) {
    std::string rangeStr = "is not in range of";
    BOOST_TEST(errorMsg.find(rangeStr) != std::string::npos);
//    std::cout << "errMsg = " + errorMsg << std::endl;
//    std::cout << "rangeStr = " + rangeStr << std::endl;
    errorCounter = errorCounter > 0 ? errorCounter - 1 : 0;
}

void onStreamEvent(const sdk::StreamLog &a_StreamLog, void *a_pUserData) {
    auto pUserData = static_cast<UserData *>(
            a_pUserData);

    std::lock_guard <std::mutex> lock(pUserData->Mutex);

    switch (a_StreamLog.Event) {
        case sdk::StreamEvent::Started:
            std::cout << "Stream " << a_StreamLog.StreamId.toString() << " started" << std::endl;
            break;
        case sdk::StreamEvent::Connected:
            std::cout << "Stream " << a_StreamLog.StreamId.toString() << " connected" << std::endl;
            if (runningStreams.find(a_StreamLog.StreamId.toString()) == runningStreams.end())
                runningStreams.insert({a_StreamLog.StreamId.toString(), true});
            pUserData->StreamState = true;
            break;
        case sdk::StreamEvent::Stopped:
            std::cout << "Stream " << a_StreamLog.StreamId.toString() << " stopped" << std::endl;
            if (runningStreams.find(a_StreamLog.StreamId.toString()) != runningStreams.end()) {
                runningStreams.find(a_StreamLog.StreamId.toString())->second = false;
            } else {
                runningStreams.insert({a_StreamLog.StreamId.toString(), false});
            }
            pUserData->StreamState = false;
            break;
        case sdk::StreamEvent::Error:
            streamErrorTest(a_StreamLog.Text, a_StreamLog.StreamId.toString());
            if (runningStreams.find(a_StreamLog.StreamId.toString()) != runningStreams.end()) {
                runningStreams.find(a_StreamLog.StreamId.toString())->second = false;
            } else {
                runningStreams.insert({a_StreamLog.StreamId.toString(), false});
            }
            std::cout << "Stream " << a_StreamLog.StreamId.toString() << " got error at module "
                      << a_StreamLog.ModuleName.toString() << ": " << a_StreamLog.Text.toString() << std::endl;
            break;
        case sdk::StreamEvent::EOS:
            std::cout << "Stream " << a_StreamLog.StreamId.toString() << " got end-of-stream" << std::endl;
            break;
    }
    pUserData->Condition.notify_one();
}

void onServerStateChange(sdk::ServerState a_eServerState, void *a_pUserData) {
    switch (a_eServerState) {
        case sdk::ServerState::ServerUp:
            std::cout << "Server is up" << std::endl;
            break;
        case sdk::ServerState::ServerDown:
            std::cout << "Server is down" << std::endl;
            break;
        case sdk::ServerState::InvalidVersion:
            std::cout << "Invalid server version" << std::endl;
            break;
    }

    // notify server state

    auto pUserData = static_cast<UserData *>(
            a_pUserData);

    {
        std::lock_guard <std::mutex> lock(pUserData->Mutex);
        pUserData->ServerState = a_eServerState == sdk::ServerState::ServerUp;
    }

    pUserData->Condition.notify_one();
}


/// \run
/// The first function we call in our test cases to configure all necessary settings

void run() {
    boost::unit_test::unit_test_log.set_threshold_level(boost::unit_test::log_messages);

//     seaMwirVideo = "/run/user/1000/gvfs/sftp:host=172.12.10.12,user=pipster/var/nfs/sharedrive/inputs/GA_Test_set/MWIR/Navel_thermal.avi";
//     seaSwirVideo = "/run/user/1000/gvfs/sftp:host=172.12.10.12,user=pipster/var/nfs/sharedrive/inputs/GA_Test_set/MWIR/Navel_thermal.avi";
//     groundMwirVideo = "/run/user/1000/gvfs/sftp:host=172.12.10.12,user=pipster/var/nfs/sharedrive/inputs/Weapon_station_videos_refael/Flir_minipop.mp4";
//     groundRgbVideo = "/run/user/1000/gvfs/sftp:host=172.12.10.12,user=pipster/var/nfs/sharedrive/inputs/GA_Test_set/RGB/ground_rgb_test_set.avi";
//     groundSwirVideo = "/run/user/1000/gvfs/sftp:host=172.12.10.12,user=pipster/var/nfs/sharedrive/inputs/GA_Test_set/Ground_Swir/ground_swir_test_set.MOV";

    seaMwirVideo = "/home/lior.lakay/Desktop/sdk_runner/test_videos/Sea_MWIR/Navel_thermal.avi";
    seaSwirVideo = "/home/lior.lakay/Desktop/sdk_runner/test_videos/Sea_MWIR/Navel_thermal.avi";
    groundMwirVideo = "/home/lior.lakay/Desktop/sdk_runner/test_videos/Ground_MWIR/Flir_minipop.mp4";
    groundRgbVideo = "/home/lior.lakay/Desktop/sdk_runner/test_videos/Ground_RGB/ground_rgb_test_set.avi";
    groundSwirVideo = "/home/lior.lakay/Desktop/sdk_runner/test_videos/Ground_SWIR/ground_swir_test_set.MOV";

    rotateAngleNotInRange = "is not in range of 0 and 270";
    rotateAngleUnsupported = "Unsupported rotate angle";
    ROINotInRange = "is not in range of 0 and 8192";
    ROIUnsupported = "Invalid ROI";
    previewError = "Failed to open display; can't use CVPreview";
//    currentRangeError = rangeError::None; // TODO -check if needed
    a_strVideoPath = seaMwirVideo;
//    a_strVideoPath = "rtsp://root:password@172.12.10.199/axis-media/media.amp";
    a_strServerIP = "172.12.10.33";
    sinkStr = "rtsp://172.12.60.124:8554/sdk_test";
    currentDetectedGroups = seaGroups;
    videoCapture.open(a_strVideoPath);

    std::cout << "Server IP: " << a_strServerIP << std::endl;
    std::cout << "Video input: " << a_strVideoPath << std::endl;
    std::cout << "Sink URL: " << sinkStr << std::endl;

    matFrame = cv::Mat::zeros(480, 640, CV_8UC3);

    sdk::GrpcSettings grpcSettings;
    grpcSettings.ServerIP = a_strServerIP;
//    grpcSettings.TimeoutInMsecs = 500;

    sdk::Callbacks callbacks;
    callbacks.OnMessage = &onMessage;
    callbacks.OnFrameResults = &onFrameResults;
    callbacks.OnStreamEvent = &onStreamEvent;
    callbacks.OnServerStateChange = &onServerStateChange;
    callbacks.UserData = &data;
    callbacks.MinLogLevel = sdk::LogLevel::Error;

    mainPipeline = sdk::Pipeline::create(
            grpcSettings,
            {}, // dds settings
            {}, // raw stream settings
            callbacks);

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

namespace std //NOLINT
{
    ostream &operator<<(ostream &os, const sdk::FlowSwitcherFlowId flowId) //NOLINT
    {
        switch (flowId) {
            case sdk::FlowSwitcherFlowId::SeaMwir:
                return os << "SeaMwir";
            case sdk::FlowSwitcherFlowId::SeaSwir:
                return os << "SeaSwir";
            case sdk::FlowSwitcherFlowId::GroundMwir:
                return os << "GroundMwir";
            case sdk::FlowSwitcherFlowId::GroundRgbAndSwir:
                return os << "GroundRgbSwir";
//            case sdk::FlowSwitcherFlowId::GroundSwir:
//                return os << "GroundSwir";
            default:
                return os << "";
        }
    }

    ostream &operator<<(ostream &os, const sdk::InitScoreMetric initScoreMetric) //NOLINT
    {
        return os << initScoreMetric;
    }

    ostream &operator<<(ostream &os, const sdk::String &str) //NOLINT
    {
        return os << str.toString();
    }

    ostream &operator<<(ostream &os, const sdk::PublisherDataType publisherDataType) //NOLINT
    {
        switch (publisherDataType) {
            case sdk::PublisherDataType::Tracks:
                return os << "Tracks";
            case sdk::PublisherDataType::Detections:
                return os << "Detections";
            case sdk::PublisherDataType::Disabled:
                return os << "Disabled";
            case sdk::PublisherDataType::PointTracks:
                return os << "PointTracks";
            case sdk::PublisherDataType::Unspecified:
                return os << "Unspecified";
            default:
                return os << "";
        }

    }

    ostream &operator<<(ostream &os, const sdk::NormalizationType &normalizationType) //NOLINT
    {
        return os << normalizationType;
    }

    ostream &operator<<(ostream &os, const sdk::Corner corner) //NOLINT
    {
        return os << corner;
    }

    ostream &operator<<(ostream &os, const sdk::OsdValue osdValue) //NOLINT
    {
        return os << osdValue;
    }

    ostream &operator<<(ostream &os, const sdk::RawSourceMode rawSourceMode) //NOLINT
    {
        return os << rawSourceMode;
    }

    ostream &operator<<(ostream &os, const sdk::DeviceAccess deviceAccess) //NOLINT
    {
        return os << deviceAccess;
    }

}

/// \SendingSingleFrame
/// Function we call to send single frame to Meerkat enginee
/// \param stream The stream we started
void sendSingleFrame(const std::shared_ptr<sdk::Stream> &stream) {

    currentFrameDimensions = {
            (float) matFrame.rows,
            (float) matFrame.cols
    };
    currentRoi = {
            0,
            0,
            (float) matFrame.rows,
            (float) matFrame.cols
    };
    if (data.StreamState) {
        stream->pushRawFrame(
                sdk::RawSourceId::RawSource,
                matFrame.data,
                matFrame.cols,
                matFrame.rows,
                sdk::PixelFormat::BGR8,
                matFrame.step[0],
                0,
                -1
        );
    }
}

/// \sendFrames
/// The function we call in order to send sequence of raw frames to Meerkat enginee
/// \param stream The stream we want send frames to
/// \param numOfFramesToSend The number of frames we send to the stream
/// \param func Reference to the function we want to execute during sending frames
void sendFrames(const std::shared_ptr<sdk::Stream> &stream, size_t numOfFramesToSend,
                const std::function<void(uint32_t)> &func) {
    numOfFrames = numOfFramesToSend;

//    cv::VideoCapture capture(a_strVideoPath);
    videoCapture.open(a_strVideoPath);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    uint32_t nFrameId = 0;
    cv::Mat mat;
    if (currentStream->getId().toString() != stream->getId().toString()) {
        currentStream = stream;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    BOOST_TEST_MESSAGE("Video file: " + a_strVideoPath);
    std::string framesToSend = numOfFramesToSend != -1 ? std::to_string(numOfFramesToSend) : "all video";
    BOOST_TEST_MESSAGE("Sending " + framesToSend + " frames, to stream ID: " + stream->getId().toString());
    while (videoCapture.read(mat)) {
        // Initiate frame size and ROI
        if (nFrameId == 0) {
            currentFrameDimensions = {
                    (float) mat.rows,
                    (float) mat.cols
            };
            currentRoi = {
                    0,
                    0,
                    (float) mat.rows,
                    (float) mat.cols
            };
        }
        // push frame
        if (runningStreams.find(stream->getId().toString())->second) {
            try {
                stream->pushRawFrame(
                        sdk::RawSourceId::RawSource, // ids are defined in Configuration.h
                        mat.data,
                        mat.cols,
                        mat.rows,
                        sdk::PixelFormat::BGR8, // supported pixel formats are defined in Types.h
                        mat.step[0],
                        nFrameId++, // can be default (-1) to use frame ids automatically generated by the engine
                        -1);        // timestamp in ms, if -1 current time is used

                std::this_thread::sleep_for(
                        std::chrono::milliseconds(50));

                if (func != nullptr)
                    func(nFrameId);
                if (nFrameId > numOfFramesToSend && numOfFramesToSend != -1) {
                    break;
                }
            }
            catch (sdk::Exception &e) {
                BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos,
                           "Caught unexpected error: " + (std::string) e.what());
            }
        } else {
//            BOOST_TEST_MESSAGE("Stream not started!");
            break;
        }
    }
}

/// \waitForStreamStarted
/// Function that stop our program process until stream starting
/// \param stream The stream we waiting to
void waitForStreamStarted(const std::shared_ptr<sdk::Stream> &stream) {
    std::cout << "\nWaiting for stream: " << stream->getId().toString() << std::endl;
    std::unique_lock<std::mutex> lock(data.Mutex);
    data.Condition.wait_for(lock, std::chrono::seconds(10), []() { return data.StreamState; });
    if (!data.StreamState) {
        std::cerr << "Couldn't start stream for 10 seconds, exiting" << std::endl;
        throw std::exception();
    }
    currentStream = stream;
}

//void waitForStreamStopped(const std::shared_ptr<sdk::Stream>& stream){ // TODO - try and fix this function
//    std::cout << "\nWaiting for stream: " << stream->getId().toString() << " to stop" << std::endl;
//    std::unique_lock<std::mutex> lock(data.Mutex);
//    data.Condition.wait_for(lock, std::chrono::seconds(10), []() { return data.StreamState; });
//    if (!data.StreamState) {
//        std::cerr << "Couldn't start stream for 10 seconds, exiting" << std::endl;
////        exit(-1);
//        throw std::exception();
//    }
//}

/// \getStartConfiguration
/// Returns start stream configurations
/// \param customSettings stream configurations, default values assigned if not sent customized settings
/// \return Start stream configurations
sdk::StartStreamConfiguration getStartConfiguration(const std::string &customSettings = "stream_settings.bin") {
    sdk::StartStreamConfiguration startConfiguration = mainPipeline->createStartStreamConfiguration(customSettings);
    startConfiguration.getRawSource();
    startConfiguration.getRawSource().setReadTimeoutInSec(-1); // TODO(NOTE) - it was 20;
    startConfiguration.getGstSink().setUrl(sinkStr);
    return startConfiguration;
}

/// \rotateAngleTestFunc
/// Test function for rotate angle mechanism
/// \param flowId The flow id we working with
void rotateAngleTestFunc(sdk::FlowSwitcherFlowId flowId){
    std::cout << printDetectorFlow[flowId] << std::endl;
    sdk::StartStreamConfiguration startConfiguration;
    errorCounter = 0;

    BOOST_TEST_MESSAGE("\nRotate angle valid angle test\n");
    try {
        startConfiguration = getStartConfiguration();
        BOOST_TEST_MESSAGE(startConfiguration.getPreprocessor().minRotateAngle());
        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        sendFrames(stream, 15,
                   [&stream](uint32_t nFrameId) {
                       if (nFrameId == 5)
                           BOOST_TEST(stream->getFullConfiguration().getPreprocessor().getRotateAngle() == 0);
                   }
        );

        int angle;
        for (int i = 1; i < 4; i++) {
            try {

                angle = i * 90;
                BOOST_TEST_MESSAGE(angle);
                startConfiguration = getStartConfiguration();
                startConfiguration.getPreprocessor().setRotateAngle(angle);
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                stream = mainPipeline->startStream(startConfiguration);
                waitForStreamStarted(stream);
                sendFrames(stream, 15, [&stream, angle](uint32_t nFrameId) {
                               if (nFrameId == 5)
                                   BOOST_TEST(stream->getFullConfiguration().getPreprocessor().getRotateAngle() == angle);
                           }
                );
            }
            catch (sdk::Exception &e) {
                BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos,
                           "Caught unexpected error: " + (std::string) e.what());
            }
        }
        stream.reset();
    }
    catch (sdk::Exception& e) {
        BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos,
                   "Caught unexpected error: " + (std::string) e.what());
    }

    BOOST_TEST_MESSAGE("\nRotate angle out of range test\n");

    try {
        startConfiguration = getStartConfiguration();
        BOOST_TEST_MESSAGE(startConfiguration.getPreprocessor().minRotateAngle() - 1);
        errorCounter++;
        startConfiguration.getPreprocessor().setRotateAngle(startConfiguration.getPreprocessor().minRotateAngle() - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    try {
        startConfiguration = getStartConfiguration();
        BOOST_TEST_MESSAGE(startConfiguration.getPreprocessor().maxRotateAngle() + 1);
        errorCounter++;
        startConfiguration.getPreprocessor().setRotateAngle(startConfiguration.getPreprocessor().maxRotateAngle() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    BOOST_TEST(errorCounter == 0, "Not all expected errors returned! Number of missed errors: " + std::to_string(errorCounter));
//    currentRangeError = rangeError::None; // TODO - check if needed

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    BOOST_TEST_MESSAGE("\nRotate angle unsupported test\n");
    int angle;
    for (int i = 0; i < 9; i++) {
        try {
            currentStreamError = streamError::RotateAngleUnsupported;
            currentErrorStr = rotateAngleUnsupported;
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
            angle = (int) (random() % 270) + 1;
            BOOST_TEST_MESSAGE(angle);
            startConfiguration = getStartConfiguration();
            startConfiguration.getPreprocessor().setRotateAngle(angle);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            errorCounter++;
            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
            if (angle % 90 == 0) {
                currentStreamError = streamError::NONE;
                errorCounter = errorCounter > 0 ? errorCounter - 1 : 0;
            } else {
                waitForStreamStarted(stream);
                sendFrames(stream, 10, nullptr);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//                    currentStreamError = streamError::RotateAngleNotInRange;
        }
        catch (sdk::Exception &e) {
            BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos,
                       "Caught unexpected error: " + (std::string) e.what());
        }
    }
    BOOST_TEST(errorCounter == 0, "Not all expected errors returned! Number of missed errors: " + std::to_string(errorCounter));
}

/// \roiTestFunc
/// Test function for start stream ROI settings
/// \param flowId The flow id we working with
template<typename CONF, typename T>
void roiTestFunc(sdk::FlowSwitcherFlowId flowId) {

    std::cout << printDetectorFlow[flowId] << std::endl;
    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
    CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
    errorCounter = 0;
    isRoiTest = true;
    BOOST_TEST_MESSAGE("\nValid ROI test\n");

    /**
    * Test valid ROI for 100 frames
    */
    try {
        startConfiguration = getStartConfiguration();
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        //TODO - implement here random valid roi depends on currentFrameDimension.Width/Height
        conf.getRoi().setX(100);
        conf.getRoi().setY(100);
        conf.getRoi().setHeight(100);
        conf.getRoi().setWidth(100);
        currentRoi = {
                100,
                100,
                100,
                100
        };
        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        sendFrames(stream, 100, nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//        waitForStreamStopped(stream); // TODO - implement stop stream func depends on threading
//        stream.reset();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    catch (sdk::Exception &e) {
        BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos, "Caught unexpected error: " + (std::string) e.what());
    }

    /**
    * Test valid ROI for 100 frames with height/width set to default
    */
    try {
        startConfiguration = getStartConfiguration();
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        conf.getRoi().setX(100);
        conf.getRoi().setY(100);
        currentRoi = {
                (float) conf.getRoi().getX(),
                (float) conf.getRoi().getY(),
                currentFrameDimensions.Width - (float)(conf.getRoi().getX()),
                currentFrameDimensions.Height - (float)(conf.getRoi().getX())
        };
        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        sendFrames(stream, 100, nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        stream.reset();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    catch (sdk::Exception &e) {
        BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos,
                   "Caught unexpected error: " + (std::string) e.what());
    }


    BOOST_TEST_MESSAGE("\nOuf of range ROI test\n");
    errorCounter = 0;
    startConfiguration = getStartConfiguration();
    conf = getDetectorConfiguration(startConfiguration, (T) 1.0);

    int minHeight = conf.getRoi().minHeight();
    size_t maxHeight = conf.getRoi().maxHeight();
    int minWidth = conf.getRoi().minWidth();
    size_t maxWidth = conf.getRoi().maxWidth();
    int minX = conf.getRoi().minX();
    size_t maxX = conf.getRoi().maxX();
    int minY = conf.getRoi().minY();
    size_t maxY = conf.getRoi().maxY();


    try {
        conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        BOOST_TEST_MESSAGE("Height=" + std::to_string(maxHeight + 1));
        errorCounter++;
        conf.getRoi().setHeight(maxHeight + 1);
    }
    catch(const sdk::Exception& e){
        outOfRangeErrorTest((std::string) e.what());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    try{
        startConfiguration = getStartConfiguration();
        conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        BOOST_TEST_MESSAGE("Height=" + std::to_string(minHeight - 1));
        errorCounter++;
        conf.getRoi().setHeight(minHeight - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());

    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    try{
        startConfiguration = getStartConfiguration();
        conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        BOOST_TEST_MESSAGE("Width=" + std::to_string(maxWidth + 1));
        errorCounter++;
        conf.getRoi().setWidth(maxWidth + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    try{
        startConfiguration = getStartConfiguration();
        conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        BOOST_TEST_MESSAGE("Width=" + std::to_string(minWidth - 1));
        errorCounter++;
        conf.getRoi().setWidth(minWidth - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());

    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    try{
        startConfiguration = getStartConfiguration();
        conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        BOOST_TEST_MESSAGE("X=" + std::to_string(maxX + 1));
        errorCounter++;
        conf.getRoi().setX(maxX + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());

    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    try{
        startConfiguration = getStartConfiguration();
        conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        BOOST_TEST_MESSAGE("X=" + std::to_string(minX - 1));
        errorCounter++;
        conf.getRoi().setX(minX - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());

    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    try{
        startConfiguration = getStartConfiguration();
        conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        BOOST_TEST_MESSAGE("Y=" + std::to_string(maxY + 1));
        errorCounter++;
        conf.getRoi().setY(maxY + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());

    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    try{
        startConfiguration = getStartConfiguration();
        conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        BOOST_TEST_MESSAGE("Y=" + std::to_string(minY - 1));
        errorCounter++;
        conf.getRoi().setY(minY - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    BOOST_TEST(errorCounter == 0, "Not all Out of range ROI expected errors returned! Number of missed errors: " + std::to_string(errorCounter));

    BOOST_TEST_MESSAGE("\nUnsupported ROI test\n");
    errorCounter = 0;
    startConfiguration = getStartConfiguration();
    currentStreamError = streamError::RoiIncorrectSize;
    currentErrorStr = ROIUnsupported;
    conf = getDetectorConfiguration(startConfiguration, (T) 1.0);

    /**
     * width out of frame
     */
    try {
        startConfiguration = getStartConfiguration();
        conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        conf.getRoi().setX(100);
        conf.getRoi().setWidth((uint32_t)(100 + currentFrameDimensions.Width));
        conf.getRoi().setHeight(1);
        errorCounter++;
        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        sendFrames(stream, 50, nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    catch (sdk::Exception &e) {
        BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos,
                   "Caught unexpected error: " + (std::string) e.what());
    }

    /**
     * height out of frame
     */
    try {
        startConfiguration = getStartConfiguration();
        conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        conf.getRoi().setY(100);
        conf.getRoi().setHeight((uint32_t)(100 + currentFrameDimensions.Height));
        errorCounter++;
        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        sendFrames(stream, 50, nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    catch (sdk::Exception &e) {
        BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos,
                   "Caught unexpected error: " + (std::string) e.what());
    }

    /**
     * X out of frame
     */

    try {
        startConfiguration = getStartConfiguration();
        conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        conf.getRoi().setX((uint32_t)(100 + currentFrameDimensions.Width));
        conf.getRoi().setY(1);
        errorCounter++;
        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        sendFrames(stream, 50, nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    catch (sdk::Exception &e) {
        BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos,
                   "Caught unexpected error: " + (std::string) e.what());
    }

    /**
     * Y out of frame
     */

    try {
        startConfiguration = getStartConfiguration();
        conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        conf.getRoi().setY((uint32_t)(100 + currentFrameDimensions.Height));
        conf.getRoi().setX(1);
        errorCounter++;
        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        sendFrames(stream, 50, nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    catch (sdk::Exception &e) {
        BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos,
                   "Caught unexpected error: " + (std::string) e.what());
    }
    BOOST_TEST(errorCounter == 0, "Not all unsupported ROI expected errors returned! Number of missed errors: " + std::to_string(errorCounter));
    isRoiTest = false;
}

/// \updateRoiTestFunc
/// Test function for updating stream ROI settings
/// \param flowId The flow id we working with
template<typename CONF, typename T>
void updateRoiTestFunc(sdk::FlowSwitcherFlowId flowId) {
    std::cout << printDetectorFlow[flowId] << std::endl;
    errorCounter = 0;
    isRoiTest = true;

    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
    startConfiguration.getFlowSwitcher().setFlowId(flowId);
    std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
    waitForStreamStarted(stream);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    CONF conf = getDetectorUpdateConfiguration(stream, (T) 1.0);

    //TODO - change all values to more generic method (if preprocessor will "delete")
    int minHeight = conf.getRoi().minHeight();
    size_t maxHeight = conf.getRoi().maxHeight();
    int minWidth = conf.getRoi().minWidth();
    size_t maxWidth = conf.getRoi().maxWidth();
    int minX = conf.getRoi().minX();
    size_t maxX = conf.getRoi().maxX();
    int minY = conf.getRoi().minY();
    size_t maxY = conf.getRoi().maxY();

    BOOST_TEST_MESSAGE("\nValid Roi update test\n");
    try{
//        BOOST_TEST_MESSAGE("(X:100, Y:100, W:100, H:100)");
        conf.getRoi().setX(100);
        conf.getRoi().setY(100);
        conf.getRoi().setWidth(100);
        conf.getRoi().setHeight(100);

        currentRoi = {
            (float) conf.getRoi().getX(),
            (float) conf.getRoi().getY(),
            (float) conf.getRoi().getWidth(),
            (float) conf.getRoi().getHeight()
        };

        stream->update();

        sendFrames(stream, 100, [&conf](uint32_t nFrameId){
           if (nFrameId == 10) {
               BOOST_TEST(conf.getRoi().getX() == 100);
               BOOST_TEST(conf.getRoi().getY() == 100);
               BOOST_TEST(conf.getRoi().getWidth() == 100);
               BOOST_TEST(conf.getRoi().getHeight() == 100);
           }
        });
    }
    catch (const sdk::Exception& e) {
        errorCounter++;
        std::cerr << "Caught unexpected error: " + (std::string) e.what() << std::endl;
    }
    try{
        BOOST_TEST_MESSAGE("(X:100, Y:100, W:100, H:100)");
        conf.getRoi().setX(100);
        conf.getRoi().setY(100);

        currentRoi = {
                (float) conf.getRoi().getX(),
                (float) conf.getRoi().getY(),
                currentFrameDimensions.Width - (float)(conf.getRoi().getX()),
                currentFrameDimensions.Height - (float)(conf.getRoi().getX())
        };

        stream->update();

        sendFrames(stream, 100, [&conf](uint32_t nFrameId){
            if (nFrameId == 10) {
                BOOST_TEST(conf.getRoi().getX() == 100);
                BOOST_TEST(conf.getRoi().getY() == 100);
            }
        });
    }
    catch (const sdk::Exception& e) {
        errorCounter++;
        std::cerr << "Caught unexpected error: " + (std::string) e.what() << std::endl;
    }
    BOOST_TEST(errorCounter == 0, "Not all valid update ROI expected errors returned! Number of missed errors: " + std::to_string(errorCounter));

    BOOST_TEST_MESSAGE("\nOut of range ROI update test\n");
    errorCounter = 0;
    try{
        BOOST_TEST_MESSAGE("X=" + std::to_string(minX - 1));
        errorCounter++;
        conf.getRoi().setX(minX - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        BOOST_TEST_MESSAGE("X=" + std::to_string(maxX + 1));
        errorCounter++;
        conf.getRoi().setX(maxX + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        BOOST_TEST_MESSAGE("Y=" + std::to_string(minY -1));
        errorCounter++;
        conf.getRoi().setY(minY - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        BOOST_TEST_MESSAGE("Y=" + std::to_string(maxY + 1));
        errorCounter++;
        conf.getRoi().setY(maxY + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        BOOST_TEST_MESSAGE("Width=" + std::to_string(minWidth - 1));
        errorCounter++;
        conf.getRoi().setWidth(minWidth - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        BOOST_TEST_MESSAGE("Width=" + std::to_string(maxWidth + 1));
        errorCounter++;
        conf.getRoi().setWidth(maxWidth + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        BOOST_TEST_MESSAGE("Height=" + std::to_string(minHeight - 1));
        errorCounter++;
        conf.getRoi().setHeight(minHeight - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        BOOST_TEST_MESSAGE("Height=" + std::to_string(maxHeight + 1));
        errorCounter++;
        conf.getRoi().setHeight(maxHeight + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    BOOST_TEST(errorCounter == 0, "Not all update ROI out of range expected errors returned! Number of missed errors: " + std::to_string(errorCounter));


    BOOST_TEST_MESSAGE("\nUnsupported ROI update test\n");
    errorCounter = 0;
    currentErrorStr = ROIUnsupported;
    currentStreamError = streamError::RoiIncorrectSize;

    /**
     * Width out of frame
     */
    try{
        errorCounter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        startConfiguration = getStartConfiguration();
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        conf = getDetectorUpdateConfiguration(stream, (T) 1.0);

        sendFrames(stream, 100, [&stream, &conf](uint32_t nFrameId){
           if(nFrameId == 10){
               conf.getRoi().setWidth((uint32_t) (100 + currentFrameDimensions.Width));
               conf.getRoi().setHeight(1);
               stream->update();
           }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    catch (const sdk::Exception& e) {
        std::cerr << "Caught unexpected error: " + (std::string) e.what() << std::endl;
    }
    /**
     * Height out of frame
     */
    try{
        errorCounter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        startConfiguration = getStartConfiguration();
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        conf = getDetectorUpdateConfiguration(stream, (T) 1.0);

        sendFrames(stream, 100, [&stream, &conf](uint32_t nFrameId){
            if(nFrameId == 10){
                conf.getRoi().setHeight((uint32_t) (100 + currentFrameDimensions.Height));
                conf.getRoi().setWidth(1);
                stream->update();
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    catch (const sdk::Exception& e) {
        std::cerr << "Caught unexpected error: " + (std::string) e.what() << std::endl;
    }
    /**
     * X out of frame
     */
    try{
        errorCounter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        startConfiguration = getStartConfiguration();
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        conf = getDetectorUpdateConfiguration(stream, (T) 1.0);

        sendFrames(stream, 100, [&stream, &conf](uint32_t nFrameId){
            if(nFrameId == 10){
                conf.getRoi().setX((uint32_t) (100 + currentFrameDimensions.Width));
                conf.getRoi().setY(1);
                stream->update();
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    catch (const sdk::Exception& e) {
        std::cerr << "Caught unexpected error: " + (std::string) e.what() << std::endl;
    }
    /**
     * Y out of frame
     */
    try{
        errorCounter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        startConfiguration = getStartConfiguration();
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        conf = getDetectorUpdateConfiguration(stream, (T) 1.0);

        sendFrames(stream, 100, [&stream, &conf](uint32_t nFrameId){
            if(nFrameId == 10){
                conf.getRoi().setY((uint32_t) (100 + currentFrameDimensions.Height));
                conf.getRoi().setX(1);
                stream->update();
            }
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    catch (const sdk::Exception& e) {
        std::cerr << "Caught unexpected error: " + (std::string) e.what() << std::endl;
    }

    BOOST_TEST(errorCounter == 0, "Not all update ROI unsupported expected errors returned! Number of missed errors: " + std::to_string(errorCounter));
    isRoiTest = false;
}

/// \thresholdTestFunc
/// Test function for start and update stream threshold settings;
/// \param flowId The flow id we working with
/// \param groups Array of group names according to flow id we working with
template<typename CONF, typename T>
void thresholdTestFunc(const std::array<std::string, 4> &groups, sdk::FlowSwitcherFlowId flowId) {
    std::cout << printDetectorFlow[flowId] << std::endl;
    errorCounter = 0;

    /**
     * Threshold
     */
    for (const std::string &group: groups){
        if(group == "None")
            break;
        BOOST_TEST_MESSAGE("Testing group: " + group);
        BOOST_TEST_MESSAGE("Valid threshold");
        try{
            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
            CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
            conf.getGroups(group).setScoreThreshold(0.5);
//            BOOST_TEST(conf.getGroups(group).getScoreThreshold() == 0.5);
            startConfiguration.getFlowSwitcher().setFlowId(flowId);
            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
            waitForStreamStarted(stream);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            sendFrames(stream, 20, nullptr);
        }
        catch (const sdk::Exception& e) {
            BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos, "Caught unexpected error: " + (std::string) e.what());
        }

        BOOST_TEST_MESSAGE("Out of range threshold");
        try {
            BOOST_TEST_MESSAGE("1.1");
            errorCounter++;
//            currentRangeError = rangeError::Threshold; // TODO -check if needed
            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
            CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
            conf.getGroups(group).setScoreThreshold(1.1);
        }
        catch (const sdk::Exception& e) {
            outOfRangeErrorTest((std::string) e.what());
        }

        try{
            BOOST_TEST_MESSAGE("0");
            errorCounter++;
            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
            CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
            conf.getGroups(group).setScoreThreshold(-1);
        }
        catch (const sdk::Exception& e) {
            outOfRangeErrorTest((std::string) e.what());
        }
        
        BOOST_TEST_MESSAGE("Updating threshold test");
        try{
            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
            startConfiguration.getFlowSwitcher().setFlowId(flowId);
            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
            waitForStreamStarted(stream);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            auto conf = getDetectorUpdateConfiguration(stream, (T) 1.0);

            sendFrames(stream, 600,
                       [&stream, &conf, &group](uint32_t nFrameId) {
                           float threshold = (float) ((random() % 91) + 10) / 100;
                           if (nFrameId == 50) {
                               BOOST_TEST_MESSAGE("\n" << threshold);
                               conf.getGroups(group).setScoreThreshold(threshold);
                               stream->update();
                               currentStream = stream;
                               BOOST_TEST(conf.getGroups(group).getScoreThreshold() == threshold);
                               std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                           } else if (nFrameId == 100) {
                               threshold = (float) ((random() % 91) + 10) / 100;
                               BOOST_TEST_MESSAGE("\n" << threshold);
                               conf.getGroups(group).setScoreThreshold(threshold);
                               stream->update();
                               currentStream = stream;
                               BOOST_TEST(conf.getGroups(group).getScoreThreshold() == threshold);
                               std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                           } else if (nFrameId == 150) {
                               threshold = (float) ((random() % 91) + 10) / 100;
                               BOOST_TEST_MESSAGE("\n" << threshold);
                               conf.getGroups(group).setScoreThreshold(threshold);
                               stream->update();
                               currentStream = stream;
                               BOOST_TEST(conf.getGroups(group).getScoreThreshold() == threshold);
                               std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                           } else if (nFrameId == 200) {
                               threshold = (float) ((random() % 91) + 10) / 100;
                               BOOST_TEST_MESSAGE("\n" << threshold);
                               conf.getGroups(group).setScoreThreshold(threshold);
                               stream->update();
                               currentStream = stream;
                               BOOST_TEST(conf.getGroups(group).getScoreThreshold() == threshold);
                               std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                           } else if (nFrameId == 250){
                               threshold = (float) ((random() % 91) + 10) / 100;
                               BOOST_TEST_MESSAGE("\n" << threshold);
                               conf.getGroups(group).setScoreThreshold(threshold);
                               stream->update();
                               currentStream = stream;
                               BOOST_TEST(conf.getGroups(group).getScoreThreshold() == threshold);
                               std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                           } else if (nFrameId == 300) {
                               threshold = (float) ((random() % 91) + 10) / 100;
                               BOOST_TEST_MESSAGE("\n" << threshold);
                               conf.getGroups(group).setScoreThreshold(threshold);
                               stream->update();
                               currentStream = stream;
                               BOOST_TEST(conf.getGroups(group).getScoreThreshold() == threshold);
                               std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                           } else if (nFrameId == 400) {
                               threshold = (float) ((random() % 91) + 10) / 100;
                               BOOST_TEST_MESSAGE("\n" << threshold);
                               conf.getGroups(group).setScoreThreshold(threshold);
                               stream->update();
                               currentStream = stream;
                               BOOST_TEST(conf.getGroups(group).getScoreThreshold() == threshold);
                               std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                           } else if (nFrameId == 450) {
                               try{
                                   errorCounter++;
                                   conf.getGroups(group).setScoreThreshold(1.1);
                               }
                               catch (const sdk::Exception& e) {
                                   outOfRangeErrorTest((std::string) e.what());
                               }
                           } else if (nFrameId == 500){
                               try{
                                   errorCounter++;
                                   conf.getGroups(group).setScoreThreshold(0);
                               }
                               catch (const sdk::Exception& e) {
                                   outOfRangeErrorTest((std::string) e.what());
                               }
                           }
                       }
            );
        }
        catch (const sdk::Exception& e) {
            BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos, "Caught unexpected error: " + (std::string) e.what());
        }

        BOOST_TEST(errorCounter == 0, "Not all expected errors returned! Number of missed errors: " + std::to_string(errorCounter));
    }
}

/// \minMaxTestFunc
/// Test function for all detector's min-max values
/// \param flowId The flow id we working with
/// \param groups Array of group names according to flow id we working with
template<typename CONF, typename T>
void minMaxTestFunc(const std::array<std::string, 4>& groups, sdk::FlowSwitcherFlowId flowId){
    std::cout << printDetectorFlow[flowId] << std::endl;
    errorCounter = 0;

    for (const std::string &group: groups){
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        CONF conf = getDetectorConfiguration(startConfiguration, (T)1.0);

        if(group == "None")
            break;
        BOOST_TEST_MESSAGE("Testing group: " + group);
        BOOST_TEST_MESSAGE("Valid test");
        try{
            float minVal = (float) (random() % 501);
            float maxVal = (float) ((random() % 501) + 500);

            BOOST_TEST_MESSAGE("min value = " + std::to_string(minVal));
            BOOST_TEST_MESSAGE("max value = " + std::to_string(maxVal));

            conf.getGroups(group).setMinWidth(minVal);
            conf.getGroups(group).setMaxWidth(maxVal);
            conf.getGroups(group).setMinHeight(minVal);
            conf.getGroups(group).setMaxHeight(maxVal);
            conf.getGroups(group).setMinAspectRatio(minVal);
            conf.getGroups(group).setMaxAspectRatio(maxVal);

            BOOST_TEST(conf.getGroups(group).getMinWidth() == minVal);
            BOOST_TEST(conf.getGroups(group).getMaxWidth() == maxVal);
            BOOST_TEST(conf.getGroups(group).getMinHeight() == minVal);
            BOOST_TEST(conf.getGroups(group).getMaxHeight() == maxVal);
            BOOST_TEST(conf.getGroups(group).getMinAspectRatio() == minVal);
            BOOST_TEST(conf.getGroups(group).getMaxAspectRatio() == maxVal);

            startConfiguration.getFlowSwitcher().setFlowId(flowId);
            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
            waitForStreamStarted(stream);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            sendFrames(stream, 20, nullptr);
        }
        catch (const sdk::Exception& e) {
            std::cerr << "Caught unexpected error: " + (std::string) e.what() << std::endl;
        }

        BOOST_TEST_MESSAGE("Out of range min/max");
        /*
         * group's min/max Width
         * */
        try {
            startConfiguration = getStartConfiguration();
            conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
            BOOST_TEST_MESSAGE("minWidth = " + std::to_string(conf.getGroups(group).minMinWidth() - 1));
            errorCounter++;
//            currentRangeError = rangeError::Threshold; // TODO - check afterwards if need add rangeError::min/max to outOfRangeFunc
            conf.getGroups(group).setMinWidth(conf.getGroups(group).minMinWidth() - 1);
        }
        catch (const sdk::Exception& e) {
            outOfRangeErrorTest((std::string) e.what());
        }

        try {
            startConfiguration = getStartConfiguration();
            conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
            BOOST_TEST_MESSAGE("minWidth = " + std::to_string(conf.getGroups(group).maxMinWidth() + 1));
            errorCounter++;
            conf.getGroups(group).setMinWidth(conf.getGroups(group).maxMinWidth() + 1);
        }
        catch (const sdk::Exception& e) {
            outOfRangeErrorTest((std::string) e.what());
        }

        try {
            startConfiguration = getStartConfiguration();
            conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
            BOOST_TEST_MESSAGE("maxWidth = " + std::to_string(conf.getGroups(group).minMaxWidth() - 1));
            errorCounter++;
            conf.getGroups(group).setMaxWidth(conf.getGroups(group).minMaxWidth() - 1);
        }
        catch (const sdk::Exception& e) {
            outOfRangeErrorTest((std::string) e.what());
        }

        try {
            startConfiguration = getStartConfiguration();
            conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
            BOOST_TEST_MESSAGE("maxWidth = " + std::to_string(conf.getGroups(group).maxMaxWidth() + 1));
            errorCounter++;
            conf.getGroups(group).setMaxWidth(conf.getGroups(group).maxMaxWidth() + 1);
        }
        catch (const sdk::Exception& e) {
            outOfRangeErrorTest((std::string) e.what());
        }
        /*
         * group's min/max Height
         * */
        try {
            startConfiguration = getStartConfiguration();
            conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
            BOOST_TEST_MESSAGE("minHeight = " + std::to_string(conf.getGroups(group).minMinHeight() - 1));
            errorCounter++;
            conf.getGroups(group).setMinHeight(conf.getGroups(group).minMinHeight() - 1);
        }
        catch (const sdk::Exception& e) {
            outOfRangeErrorTest((std::string) e.what());
        }

        try {
            startConfiguration = getStartConfiguration();
            conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
            BOOST_TEST_MESSAGE("minHeight = " + std::to_string(conf.getGroups(group).maxMinHeight() + 1));
            errorCounter++;
            conf.getGroups(group).setMinHeight(conf.getGroups(group).maxMinHeight() + 1);
        }
        catch (const sdk::Exception& e) {
            outOfRangeErrorTest((std::string) e.what());
        }

        try {
            startConfiguration = getStartConfiguration();
            conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
            BOOST_TEST_MESSAGE("maxHeight = " + std::to_string(conf.getGroups(group).minMaxHeight() - 1));
            errorCounter++;
            conf.getGroups(group).setMaxHeight(conf.getGroups(group).minMaxHeight() - 1);
        }
        catch (const sdk::Exception& e) {
            outOfRangeErrorTest((std::string) e.what());
        }

        try {
            startConfiguration = getStartConfiguration();
            conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
            BOOST_TEST_MESSAGE("maxHeight = " + std::to_string(conf.getGroups(group).maxMaxHeight() + 1));
            errorCounter++;
            conf.getGroups(group).setMaxHeight(conf.getGroups(group).maxMaxHeight() + 1);
        }
        catch (const sdk::Exception& e) {
            outOfRangeErrorTest((std::string) e.what());
        }
        /*
         * group's min/max AspectRatio
         * */
        try {
            startConfiguration = getStartConfiguration();
            conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
            BOOST_TEST_MESSAGE("minAspectRatio = " + std::to_string(conf.getGroups(group).minMinAspectRatio() - 1));
            errorCounter++;
            conf.getGroups(group).setMinAspectRatio(conf.getGroups(group).minMinHeight() - 1);
        }
        catch (const sdk::Exception& e) {
            outOfRangeErrorTest((std::string) e.what());
        }

        try {
            startConfiguration = getStartConfiguration();
            conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
            BOOST_TEST_MESSAGE("minAspectRatio = " + std::to_string(conf.getGroups(group).maxMinAspectRatio() + 1));
            errorCounter++;
            conf.getGroups(group).setMinAspectRatio(conf.getGroups(group).maxMinAspectRatio() + 1);
        }
        catch (const sdk::Exception& e) {
            outOfRangeErrorTest((std::string) e.what());
        }

        try {
            startConfiguration = getStartConfiguration();
            conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
            BOOST_TEST_MESSAGE("maxAspectRatio = " + std::to_string(conf.getGroups(group).minMaxAspectRatio() - 1));
            errorCounter++;
            conf.getGroups(group).setMaxAspectRatio(conf.getGroups(group).minMaxAspectRatio() - 1);
        }
        catch (const sdk::Exception& e) {
            outOfRangeErrorTest((std::string) e.what());
        }

        try {
            startConfiguration = getStartConfiguration();
            conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
            BOOST_TEST_MESSAGE("maxAspectRatio = " + std::to_string(conf.getGroups(group).maxMaxAspectRatio() + 1));
            errorCounter++;
            conf.getGroups(group).setMaxAspectRatio(conf.getGroups(group).maxMaxAspectRatio() + 1);
        }
        catch (const sdk::Exception& e) {
            outOfRangeErrorTest((std::string) e.what());
        }
        // TODO - finish from here - updating to oor test cases *** most important
        BOOST_TEST_MESSAGE("Update min/max test...");
        try{
            errorCounter++;
            startConfiguration = getStartConfiguration();
            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
            waitForStreamStarted(stream);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            sendFrames(stream, 50, nullptr);
            auto conf = getDetectorUpdateConfiguration(stream, (T) 1.0);
            conf.getGroups(group).setMaxAspectRatio(conf.getGroups(group).maxMaxAspectRatio() + 1);
        }
        catch (const sdk::Exception& e) {
            outOfRangeErrorTest((std::string) e.what());
        }

        BOOST_TEST(errorCounter == 0, "Not all expected errors returned! Number of missed errors: " + std::to_string(errorCounter));
    }
}

/// \trackerRateTestFunc
/// Test function for tracker's output frame rate
/// \param flowId The flow id we working with
template<typename CONF, typename T>
void trackerRateTestFunc(sdk::FlowSwitcherFlowId flowId){
    std::cout << printTrackerFlow[flowId] << std::endl;
    errorCounter = 0;

    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
    startConfiguration.getFlowSwitcher().setFlowId(flowId);
    CONF conf = getTrackerRateStartConfiguration(startConfiguration, (T) 1.0);

    BOOST_TEST_MESSAGE("Valid test");
    for(int i=0; i<3; i++) {
        try {
            uint32_t rate = (uint32_t) (random() % 1001);
            if(!rate)
                continue;
            BOOST_TEST_MESSAGE(rate);
            conf.setOutputFramerate(rate);
            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
            waitForStreamStarted(stream);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            sendFrames(stream, 20, [&conf, &rate](uint32_t nFrameId){
                if(nFrameId == 5)
                    BOOST_TEST(conf.getOutputFramerate() == rate);
            });
        }
        catch (const sdk::Exception &e) {
            std::cerr << "Caught unexpected error: " + (std::string) e.what() << std::endl;
        }
    }

    BOOST_TEST_MESSAGE("Out of range test");
    try{
        errorCounter++;
        BOOST_TEST_MESSAGE("Tracker rate = " + std::to_string((int)conf.minOutputFramerate() -1));
        startConfiguration = getStartConfiguration();
        conf = getTrackerRateStartConfiguration(startConfiguration, (T) 1.0);
        conf.setOutputFramerate(conf.minOutputFramerate() -1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        errorCounter++;
        BOOST_TEST_MESSAGE("Tracker rate = " + std::to_string(conf.maxOutputFramerate() + 1));
        startConfiguration = getStartConfiguration();
        conf = getTrackerRateStartConfiguration(startConfiguration, (T) 1.0);
        conf.setOutputFramerate(conf.maxOutputFramerate() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }


    BOOST_TEST_MESSAGE("Update test");
    try{
        errorCounter = 0;
        startConfiguration = getStartConfiguration();
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        auto conf = getTrackerRateUpdateConfiguration(stream, (T) 1.0);
        uint32_t rate;

        sendFrames(stream, 300, [&stream, &conf, &rate](uint32_t nFrameId){
            switch(nFrameId){
                case 50:
                    rate = (uint32_t) (random() % 1001);
                    BOOST_TEST_MESSAGE(rate);
                    conf.setOutputFramerate(rate);
                    stream->update();
                    BOOST_TEST(conf.getOutputFramerate() == rate);
                    break;
                case 100:
                    rate = (uint32_t) (random() % 1001);
                    BOOST_TEST_MESSAGE(rate);
                    conf.setOutputFramerate(rate);
                    stream->update();
                    BOOST_TEST(conf.getOutputFramerate() == rate);
                    break;
                case 150:
                    rate = (uint32_t) (random() % 1001);
                    BOOST_TEST_MESSAGE(rate);
                    conf.setOutputFramerate(rate);
                    stream->update();
                    BOOST_TEST(conf.getOutputFramerate() == rate);
                    break;
                case 200:
                    try{
                        errorCounter++;
                        conf.setOutputFramerate(conf.maxOutputFramerate() + 1);
//                        stream->update();
                    }
                    catch (const sdk::Exception& e) {
                        outOfRangeErrorTest((std::string) e.what());
                    }
                case 225:
                    try{
                        errorCounter++;
                        conf.setOutputFramerate(conf.minOutputFramerate() - 1);
                    }
                    catch (const sdk::Exception& e) {
                        outOfRangeErrorTest((std::string) e.what());
                    }
                    break;
            }
        });

    }
    catch (const sdk::Exception& e) {
        std::cerr << "Caught unexpected error: " + (std::string) e.what() << std::endl;
    }

    BOOST_TEST(errorCounter == 0, "Not all expected errors returned! Number of missed errors: " + std::to_string(errorCounter));
}

/// \trackerTestFunc
/// Test function for general tracker's settings
/// \param flowId The flow id we working with
template<typename CONF, typename T>
void trackerTestFunc(sdk::FlowSwitcherFlowId flowId){
    std::cout << printTrackerFlow[flowId] << std::endl;
    errorCounter = 0;
    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
    CONF conf = getTrackerStartConfiguration(startConfiguration, (T) 1.0);

    float maxFeatureWeight = conf.getParameters().maxFeatureWeight();
    float maxInitThreshold = conf.getParameters().maxInitThreshold();
    uint32_t maxInitTrackSize = conf.getParameters().maxInitTrackSize();
    float maxMaxCosineDistance = conf.getParameters().maxMaxCosineDistance();
    float maxMaxMahalanbisDistance = conf.getParameters().maxMaxMahalanobisDistance();
    uint32_t maxMaxPredictedFrames = conf.getParameters().maxMaxPredictedFrames();
    uint32_t maxTrackFeaturesHistorySize = conf.getParameters().maxTrackFeaturesHistorySize();
    float maxMinIouThreshold = conf.getParameters().maxMinIouThreshold();
    uint32_t maxMaxTimeSinceUpdateToReport = conf.maxMaxTimeSinceUpdateToReport();

    float minFeatureWeight = conf.getParameters().minFeatureWeight();
    float minInitThreshold = conf.getParameters().minInitThreshold();
    uint32_t minInitTrackSize = conf.getParameters().minInitTrackSize();
    float minMaxCosineDistance = conf.getParameters().minMaxCosineDistance();
    float minMaxMahalanbisDistance = conf.getParameters().minMaxMahalanobisDistance();
    uint32_t minMaxPredictedFrames = conf.getParameters().minMaxPredictedFrames();
    uint32_t minTrackFeaturesHistorySize = conf.getParameters().minTrackFeaturesHistorySize();
    float minMinIouThreshold = conf.getParameters().minMinIouThreshold();
    uint32_t minMaxTimeSinceUpdateToReport = conf.minMaxTimeSinceUpdateToReport();


    BOOST_TEST_MESSAGE("Valid test");
    try{
        conf.getParameters().setFeatureWeight((float)((random() % 101) / 100));
        conf.getParameters().setInitThreshold((float)((random() % 101) / 100));
        conf.getParameters().setInitTrackSize((uint32_t)(random() % (maxInitTrackSize + 1)));
        conf.getParameters().setMaxCosineDistance(((random() % (uint32_t) (maxMaxCosineDistance + 1)) / 100));
        conf.getParameters().setMaxMahalanobisDistance((random() % (uint32_t)(maxMaxMahalanbisDistance + 1)));
        conf.getParameters().setMaxPredictedFrames((uint32_t)(random() % 1001));
        conf.getParameters().setMinIouThreshold((float)((random() % 101) / 100));
        conf.getParameters().setTrackFeaturesHistorySize((random() % (maxTrackFeaturesHistorySize)) + 1);

        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        sendFrames(stream, 30, nullptr);
    }
    catch (const sdk::Exception& e) {
        std::cerr << (std::string) e.what() << std::endl;
    }

    BOOST_TEST_MESSAGE("Out of range test");
    /*
     * feature weight
     * */
    try{
        BOOST_TEST_MESSAGE("Feature weight = " + std::to_string(minFeatureWeight - 1));
        errorCounter++;
        conf.getParameters().setFeatureWeight(minFeatureWeight - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        BOOST_TEST_MESSAGE("Feature weight = " + std::to_string(maxFeatureWeight + 1));
        errorCounter++;
        conf.getParameters().setFeatureWeight(maxFeatureWeight + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    /*
     * Init threshold
     * */
    try{
        BOOST_TEST_MESSAGE("Init threshold = " + std::to_string(minInitThreshold - 1));
        errorCounter++;
        conf.getParameters().setInitThreshold(minInitThreshold - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        BOOST_TEST_MESSAGE("Init threshold = " + std::to_string(maxInitThreshold + 1));
        errorCounter++;
        conf.getParameters().setInitThreshold(maxInitThreshold + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    /*
     * Init track size
     * */
    try{
        BOOST_TEST_MESSAGE("Init track size = " + std::to_string(minInitTrackSize - 1));
        errorCounter++;
        conf.getParameters().setInitTrackSize(minInitTrackSize - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        BOOST_TEST_MESSAGE("Init track size = " + std::to_string(maxInitTrackSize + 1));
        errorCounter++;
        conf.getParameters().setInitThreshold(maxInitTrackSize + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    /*
     * MaxCosineDistance
     * */
    try{
        BOOST_TEST_MESSAGE("MaxCosineDistance = " + std::to_string(minMaxCosineDistance - 1));
        errorCounter++;
        conf.getParameters().setMaxCosineDistance(minMaxCosineDistance - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        BOOST_TEST_MESSAGE("MaxCosineDistance = " + std::to_string(maxInitTrackSize + 1));
        errorCounter++;
        conf.getParameters().setMaxCosineDistance(maxMaxCosineDistance + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    /*
     * MaxMahalanobisDistance
     * */
    try{
        BOOST_TEST_MESSAGE("MaxMahalanobisDistance = " + std::to_string(minMaxMahalanbisDistance - 1));
        errorCounter++;
        conf.getParameters().setMaxMahalanobisDistance(minMaxCosineDistance - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        BOOST_TEST_MESSAGE("MaxMahalanobisDistance = " + std::to_string(maxMaxMahalanbisDistance + 1));
        errorCounter++;
        conf.getParameters().setMaxMahalanobisDistance(maxMaxMahalanbisDistance + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    /*
     *  MaxPredictedFrames
     * */
    try{
        BOOST_TEST_MESSAGE("MaxPredictedFrames = " + std::to_string(minMaxPredictedFrames - 1));
        errorCounter++;
        conf.getParameters().setMaxPredictedFrames(minMaxPredictedFrames - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        BOOST_TEST_MESSAGE("MaxPredictedFrames = " + std::to_string(maxMaxPredictedFrames + 1));
        errorCounter++;
        conf.getParameters().setMaxPredictedFrames(maxMaxPredictedFrames + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    /*
     * TrackFeaturesHistorySize
     * */
    try{
        BOOST_TEST_MESSAGE("TrackFeaturesHistorySize = " + std::to_string(minTrackFeaturesHistorySize - 1));
        errorCounter++;
        conf.getParameters().setTrackFeaturesHistorySize(minTrackFeaturesHistorySize - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        BOOST_TEST_MESSAGE("TrackFeaturesHistorySize = " + std::to_string(maxTrackFeaturesHistorySize + 1));
        errorCounter++;
        conf.getParameters().setMaxPredictedFrames(maxTrackFeaturesHistorySize + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    /*
     * MinIouThreshold
     * */
    try{
        BOOST_TEST_MESSAGE("MinIouThreshold = " + std::to_string(minMinIouThreshold - 1));
        errorCounter++;
        conf.getParameters().setMinIouThreshold(minMinIouThreshold - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        BOOST_TEST_MESSAGE("MinIouThreshold = " + std::to_string(maxMinIouThreshold + 1));
        errorCounter++;
        conf.getParameters().setMinIouThreshold(maxMinIouThreshold + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    /*
     * MaxTimeSinceUpdateToReport
     * */
    try{
        BOOST_TEST_MESSAGE("MaxTimeSinceUpdateToReport = " + std::to_string(minMaxTimeSinceUpdateToReport - 1));
        errorCounter++;
        conf.setMaxTimeSinceUpdateToReport(minMaxTimeSinceUpdateToReport - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    try{
        BOOST_TEST_MESSAGE("MaxTimeSinceUpdateToReport = " + std::to_string(maxMaxTimeSinceUpdateToReport + 1));
        errorCounter++;
        conf.setMaxTimeSinceUpdateToReport(maxMaxTimeSinceUpdateToReport + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    BOOST_TEST_MESSAGE("Update test");
    /*
     * Valid update
     * */
    try{
        startConfiguration = getStartConfiguration();
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        auto conf = getTrackerUpdateConfiguration(stream, (T) 1.0);
        conf.setEnable(false);
        uint32_t updateVal = (uint32_t) random() % 1001;
        conf.setMaxTimeSinceUpdateToReport(updateVal);
        stream->update();
        BOOST_TEST(conf.getMaxTimeSinceUpdateToReport() == updateVal);
        sendFrames(stream, 50, nullptr);
    }
    catch (const sdk::Exception& e) {
        std::cerr << (std::string) e.what() << std::endl;
    }
    /*
     * Updating to o.o.r value
     * */
    try{
        errorCounter++;
        startConfiguration = getStartConfiguration();
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        auto conf = getTrackerUpdateConfiguration(stream, (T) 1.0);
        conf.setMaxTimeSinceUpdateToReport(maxMaxTimeSinceUpdateToReport + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }

    BOOST_TEST(errorCounter == 0, "Not all expected errors returned! Number of missed errors: " + std::to_string(errorCounter));
}

/// \rendererTestFunc
/// Test function for renderer settings
void rendererTestFunc(){
    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
    errorCounter = 0;
    BOOST_TEST_MESSAGE("Starting Out of range tests...");
    try {
        BOOST_TEST_MESSAGE("Target Width = " + std::to_string(startConfiguration.getRenderer().maxTargetWidth()+ 1));
        errorCounter++;
//        startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().setTargetWidth(startConfiguration.getRenderer().maxTargetWidth() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Target Height = " + std::to_string(startConfiguration.getRenderer().maxTargetHeight() + 1));
        errorCounter++;
//        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().setTargetHeight(startConfiguration.getRenderer().maxTargetHeight() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Normalization.Alpha = " + std::to_string(startConfiguration.getRenderer().getNormalization().maxAlpha() + 1));
        errorCounter++;
//        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getNormalization().setAlpha(startConfiguration.getRenderer().getNormalization().maxAlpha() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Normalization.Beta = " + std::to_string(startConfiguration.getRenderer().getNormalization().maxBeta() + 1));
        errorCounter++;
//        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getNormalization().setBeta(startConfiguration.getRenderer().getNormalization().maxBeta() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Normalization.StdCropSize = " + std::to_string(startConfiguration.getRenderer().getNormalization().maxStdCropSize() + 1));
        errorCounter++;
//        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getNormalization().setStdCropSize(startConfiguration.getRenderer().getNormalization().maxStdCropSize() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("TrackVelocityFactor = " + std::to_string(startConfiguration.getRenderer().maxTrackVelocityFactor() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().setTrackVelocityFactor(startConfiguration.getRenderer().maxTrackVelocityFactor() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("PointTrackRadius = " + std::to_string(startConfiguration.getRenderer().maxPointTrackRadius() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().setPointTrackRadius(startConfiguration.getRenderer().maxPointTrackRadius() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("CenterOfMassRadius = " + std::to_string(startConfiguration.getRenderer().maxCenterOfMassRadius() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().setCenterOfMassRadius(startConfiguration.getRenderer().maxCenterOfMassRadius() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("DetectorRoi.Color = " + std::to_string(startConfiguration.getRenderer().getDetectorRoi().maxColor() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getDetectorRoi().setColor(startConfiguration.getRenderer().getDetectorRoi().maxColor() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("DetectorRoi.LineThickness = " + std::to_string(startConfiguration.getRenderer().getDetectorRoi().maxLineThickness() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getDetectorRoi().setLineThickness(startConfiguration.getRenderer().getDetectorRoi().maxLineThickness() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("DetectorRoi.LineThickness = " + std::to_string(startConfiguration.getRenderer().getDetectorRoi().maxLineThickness() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getDetectorRoi().setLineThickness(startConfiguration.getRenderer().getDetectorRoi().maxLineThickness() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("BoundingBoxes[].BoxColor = " + std::to_string(startConfiguration.getRenderer().getBoundingBoxes("").maxBoxColor() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getBoundingBoxes("").setBoxColor(startConfiguration.getRenderer().getBoundingBoxes("").maxBoxColor() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("BoundingBoxes[].LineThickness = " + std::to_string(startConfiguration.getRenderer().getBoundingBoxes("").maxLineThickness() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getBoundingBoxes("").setLineThickness(startConfiguration.getRenderer().getBoundingBoxes("").maxLineThickness() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("BoundingBoxes[].TextOffsetX = " + std::to_string(startConfiguration.getRenderer().getBoundingBoxes("").maxTextOffsetX() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getBoundingBoxes("").setTextOffsetX(startConfiguration.getRenderer().getBoundingBoxes("").maxTextOffsetX() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("BoundingBoxes[].TextOffsetY = " + std::to_string(startConfiguration.getRenderer().getBoundingBoxes("").maxTextOffsetY() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getBoundingBoxes("").setTextOffsetY(startConfiguration.getRenderer().getBoundingBoxes("").maxTextOffsetY() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("BoundingBoxes[].FontColor = " + std::to_string(startConfiguration.getRenderer().getBoundingBoxes("").maxFontColor() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getBoundingBoxes("").setFontColor(startConfiguration.getRenderer().getBoundingBoxes("").maxFontColor() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("BoundingBoxes[].FontScale = " + std::to_string(startConfiguration.getRenderer().getBoundingBoxes("").maxFontScale() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getBoundingBoxes("").setFontScale(startConfiguration.getRenderer().getBoundingBoxes("").maxFontScale() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("BoundingBoxes[].FontThickness = " + std::to_string(startConfiguration.getRenderer().getBoundingBoxes("").maxFontThickness() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getBoundingBoxes("").setFontScale(startConfiguration.getRenderer().getBoundingBoxes("").maxFontThickness() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Osd.MarginX = " + std::to_string(startConfiguration.getRenderer().getOsd().maxMarginX() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getOsd().setMarginX(startConfiguration.getRenderer().getOsd().maxMarginX() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Osd.MarginY = " + std::to_string(startConfiguration.getRenderer().getOsd().maxMarginY() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getOsd().setMarginY(startConfiguration.getRenderer().getOsd().maxMarginY() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Osd.LineDistance = " + std::to_string(startConfiguration.getRenderer().getOsd().maxLineDistance() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getOsd().setMarginY(startConfiguration.getRenderer().getOsd().maxLineDistance() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Osd.BackColor = " + std::to_string(startConfiguration.getRenderer().getOsd().maxBackColor() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getOsd().setBackColor(startConfiguration.getRenderer().getOsd().maxBackColor() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Osd.BackTransparency = " + std::to_string(startConfiguration.getRenderer().getOsd().maxBackTransparency() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getOsd().setBackColor(startConfiguration.getRenderer().getOsd().maxBackTransparency() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Osd.FontColor = " + std::to_string(startConfiguration.getRenderer().getOsd().maxFontColor() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getOsd().setFontColor(startConfiguration.getRenderer().getOsd().maxFontColor() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Osd.FontScale = " + std::to_string(startConfiguration.getRenderer().getOsd().maxFontScale() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getOsd().setFontScale(startConfiguration.getRenderer().getOsd().maxFontScale() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Osd.FontThickness = " + std::to_string(startConfiguration.getRenderer().getOsd().maxFontThickness() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getOsd().setFontScale(startConfiguration.getRenderer().getOsd().maxFontThickness() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Histogram.MarginX = " + std::to_string(startConfiguration.getRenderer().getHistogram().maxMarginX() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getHistogram().setMarginX(startConfiguration.getRenderer().getHistogram().maxMarginX() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Histogram.MarginY = " + std::to_string(startConfiguration.getRenderer().getHistogram().maxMarginY() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getHistogram().setMarginY(startConfiguration.getRenderer().getHistogram().maxMarginY() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Histogram.Bins = " + std::to_string(startConfiguration.getRenderer().getHistogram().maxBins() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getHistogram().setMarginY(startConfiguration.getRenderer().getHistogram().maxBins() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Histogram.Width = " + std::to_string(startConfiguration.getRenderer().getHistogram().maxWidth() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getHistogram().setWidth(startConfiguration.getRenderer().getHistogram().maxWidth() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Histogram.Height = " + std::to_string(startConfiguration.getRenderer().getHistogram().maxHeight() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getHistogram().setHeight(startConfiguration.getRenderer().getHistogram().maxHeight() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Histogram.BackColor = " + std::to_string(startConfiguration.getRenderer().getHistogram().maxBackColor() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getHistogram().setBackColor(startConfiguration.getRenderer().getHistogram().maxBackColor() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Histogram.BackTransparency = " + std::to_string(startConfiguration.getRenderer().getHistogram().maxBackTransparency() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getHistogram().setBackTransparency(startConfiguration.getRenderer().getHistogram().maxBackTransparency() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Histogram.ColorBefore = " + std::to_string(startConfiguration.getRenderer().getHistogram().maxColorBefore() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getHistogram().setColorBefore(startConfiguration.getRenderer().getHistogram().maxColorBefore() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    try {
        BOOST_TEST_MESSAGE("Histogram.ColorAfter = " + std::to_string(startConfiguration.getRenderer().getHistogram().maxColorAfter() + 1));
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getRenderer().getHistogram().setColorAfter(startConfiguration.getRenderer().getHistogram().maxColorAfter() + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what());
    }
    BOOST_TEST(errorCounter == 0, "Not all expected errors returned! Number of missed errors: " + std::to_string(errorCounter));
}



BOOST_AUTO_TEST_CASE(run_tests) { // NOLINT
        run();
}


BOOST_AUTO_TEST_SUITE(icd_tests) // NOLINT
BOOST_AUTO_TEST_CASE(start_defaults) { // NOLINT
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();

        /*
         * Preprocessor
         */
        BOOST_TEST_MESSAGE("Starting test - Preprocessor");
        BOOST_TEST(startConfiguration.getPreprocessor().getRoi().getWidth() == 0);
        BOOST_TEST(startConfiguration.getPreprocessor().getRoi().getHeight() == 0);
        BOOST_TEST(startConfiguration.getPreprocessor().getRoi().getX() == 0);
        BOOST_TEST(startConfiguration.getPreprocessor().getRoi().getY() == 0);
        BOOST_TEST(startConfiguration.getPreprocessor().getRotateAngle() == 0);


        /*
         * Default Flow ID (Sea Mwir)
         */
        BOOST_TEST_MESSAGE("Starting test - Default Flow ID (Sea Mwir)");
        BOOST_TEST(startConfiguration.getFlowSwitcher().getFlowId() == sdk::FlowSwitcherFlowId::SeaMwir);

        /*
         * Sea Mwir Detector
         */
        BOOST_TEST_MESSAGE("Starting test - Sea Mwir Detector");
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getRoi().getWidth() == 0);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getRoi().getHeight() == 0);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getRoi().getX() == 0);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getRoi().getY() == 0);
        // ship
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("ship").getScoreThreshold() == 0.5f);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("ship").getMinWidth() == 0);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("ship").getMaxWidth() == 400);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("ship").getMinHeight() == 0);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("ship").getMaxHeight() == 400);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("ship").getMinAspectRatio() == 0.01f);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("ship").getMaxAspectRatio() == 100);
        // sailboat
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("sailboat").getScoreThreshold() == 0.5f);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("sailboat").getMinWidth() == 0);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("sailboat").getMaxWidth() == 400);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("sailboat").getMinHeight() == 0);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("sailboat").getMaxHeight() == 400);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("sailboat").getMinAspectRatio() == 0.01f);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("sailboat").getMaxAspectRatio() == 100);
        // jetski
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("jetski").getScoreThreshold() == 0.3f);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("jetski").getMinWidth() == 0);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("jetski").getMaxWidth() == 400);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("jetski").getMinHeight() == 0);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("jetski").getMaxHeight() == 400);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("jetski").getMinAspectRatio() == 0.01f);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("jetski").getMaxAspectRatio() == 100);
        // motorboat
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("motorboat").getScoreThreshold() == 0.4f);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("motorboat").getMinWidth() == 0);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("motorboat").getMaxWidth() == 400);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("motorboat").getMinHeight() == 0);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("motorboat").getMaxHeight() == 400);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("motorboat").getMinAspectRatio() == 0.01f);
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getGroups("motorboat").getMaxAspectRatio() == 100);

        /*
         * Sea Mwir Postprocessor
         */
        BOOST_TEST_MESSAGE("Starting test - Sea Mwir Postprocessor");
        BOOST_TEST(startConfiguration.getSeaMwirDetector().getOutputClasses(0) == "*");

        /*
         * Sea Mwir Tracker
         */
        BOOST_TEST_MESSAGE("Starting test - Sea Mwir Tracker");
        BOOST_TEST(startConfiguration.getSeaMwirTracker().getEnable() == true);
        BOOST_TEST(startConfiguration.getSeaMwirTracker().getMaxTimeSinceUpdateToReport() == 5);
        BOOST_TEST(startConfiguration.getSeaMwirTracker().getParameters().getInitTrackSize() == 15);
        BOOST_TEST(startConfiguration.getSeaMwirTracker().getParameters().getInitThreshold() == 0.3f);
        BOOST_TEST(startConfiguration.getSeaMwirTracker().getParameters().getInitMetric() == sdk::InitScoreMetric::Median);
        BOOST_TEST(startConfiguration.getSeaMwirTracker().getParameters().getMaxPredictedFrames() == 30);
        BOOST_TEST(startConfiguration.getSeaMwirTracker().getParameters().getTrackFeaturesHistorySize() == 1);
        BOOST_TEST(startConfiguration.getSeaMwirTracker().getParameters().getFeatureWeight() == 0);
        BOOST_TEST(startConfiguration.getSeaMwirTracker().getParameters().getMaxCosineDistance() == 0.15f);
        BOOST_TEST(startConfiguration.getSeaMwirTracker().getParameters().getMaxMahalanobisDistance() == 6);
        BOOST_TEST(startConfiguration.getSeaMwirTracker().getParameters().getMinIouThreshold() == 0.2f);

        /*
         * Sea Swir Detector
         */
        BOOST_TEST_MESSAGE("Starting test - Sea Swir Detector");
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getRoi().getWidth() == 0);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getRoi().getHeight() == 0);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getRoi().getX() == 0);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getRoi().getY() == 0);
        // ship
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("ship").getScoreThreshold() == 0.5f);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("ship").getMinWidth() == 0);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("ship").getMaxWidth() == 400);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("ship").getMinHeight() == 0);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("ship").getMaxHeight() == 400);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("ship").getMinAspectRatio() == 0.01f);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("ship").getMaxAspectRatio() == 100);
        // sailboat
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("sailboat").getScoreThreshold() == 0.5f);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("sailboat").getMinWidth() == 0);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("sailboat").getMaxWidth() == 400);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("sailboat").getMinHeight() == 0);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("sailboat").getMaxHeight() == 400);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("sailboat").getMinAspectRatio() == 0.01f);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("sailboat").getMaxAspectRatio() == 100);
        // jetski
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("jetski").getScoreThreshold() == 0.5f);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("jetski").getMinWidth() == 0);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("jetski").getMaxWidth() == 400);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("jetski").getMinHeight() == 0);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("jetski").getMaxHeight() == 400);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("jetski").getMinAspectRatio() == 0.01f);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("jetski").getMaxAspectRatio() == 100);
        // motorboat
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("motorboat").getScoreThreshold() == 0.5f);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("motorboat").getMinWidth() == 0);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("motorboat").getMaxWidth() == 400);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("motorboat").getMinHeight() == 0);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("motorboat").getMaxHeight() == 400);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("motorboat").getMinAspectRatio() == 0.01f);
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getGroups("motorboat").getMaxAspectRatio() == 100);

        /*
         * Sea Swir Postprocessor
         */
        BOOST_TEST_MESSAGE("Starting test - Sea Swir Postprocessor");
        BOOST_TEST(startConfiguration.getSeaSwirDetector().getOutputClasses(0) == "*");

        /*
         * Sea Swir Tracker
         */
        BOOST_TEST_MESSAGE("Starting test - Sea Swir Tracker");
        BOOST_TEST(startConfiguration.getSeaSwirTracker().getEnable() == true);
        BOOST_TEST(startConfiguration.getSeaSwirTracker().getMaxTimeSinceUpdateToReport() == 5);
        BOOST_TEST(startConfiguration.getSeaSwirTracker().getParameters().getInitTrackSize() == 15);
        BOOST_TEST(startConfiguration.getSeaSwirTracker().getParameters().getInitThreshold() == 0.5f);
        BOOST_TEST(startConfiguration.getSeaSwirTracker().getParameters().getInitMetric() == sdk::InitScoreMetric::Median);
        BOOST_TEST(startConfiguration.getSeaSwirTracker().getParameters().getMaxPredictedFrames() == 25);
        BOOST_TEST(startConfiguration.getSeaSwirTracker().getParameters().getTrackFeaturesHistorySize() == 1);
        BOOST_TEST(startConfiguration.getSeaSwirTracker().getParameters().getFeatureWeight() == 0);
        BOOST_TEST(startConfiguration.getSeaSwirTracker().getParameters().getMaxCosineDistance() == 0.15f);
        BOOST_TEST(startConfiguration.getSeaSwirTracker().getParameters().getMaxMahalanobisDistance() == 6);
        BOOST_TEST(startConfiguration.getSeaSwirTracker().getParameters().getMinIouThreshold() == 0.2f);

        /*
         * Ground Mwir Flow ID
         */
        startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundMwir);
        BOOST_TEST_MESSAGE("Starting test - Ground Mwir Flow ID");
        BOOST_TEST(startConfiguration.getFlowSwitcher().getFlowId() == sdk::FlowSwitcherFlowId::GroundMwir);

        /*
         * Ground Mwir Detector
         */
        BOOST_TEST_MESSAGE("Starting test - Ground Mwir Detector");
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getRoi().getWidth() == 0);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getRoi().getHeight() == 0);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getRoi().getX() == 0);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getRoi().getY() == 0);
        // light-vehicle
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("light-vehicle").getScoreThreshold() == 0.4f);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("light-vehicle").getMinWidth() == 1);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("light-vehicle").getMaxWidth() == 1000);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("light-vehicle").getMinHeight() == 1);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("light-vehicle").getMaxHeight() == 1000);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("light-vehicle").getMinAspectRatio() == 0);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("light-vehicle").getMaxAspectRatio() == 100);
        // person
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("person").getScoreThreshold() == 0.6f);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("person").getMinWidth() == 1);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("person").getMaxWidth() == 1000);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("person").getMinHeight() == 1);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("person").getMaxHeight() == 1000);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("person").getMinAspectRatio() == 0);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("person").getMaxAspectRatio() == 100);
        // two-wheeled
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("two-wheeled").getScoreThreshold() == 0.4f);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("two-wheeled").getMinWidth() == 1);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("two-wheeled").getMaxWidth() == 1000);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("two-wheeled").getMinHeight() == 1);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("two-wheeled").getMaxHeight() == 1000);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("two-wheeled").getMinAspectRatio() == 0);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("two-wheeled").getMaxAspectRatio() == 100);

        /*
         * Ground Mwir Postprocessor
         */
        BOOST_TEST_MESSAGE("Starting test - Ground Mwir Postprocessor");
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getOutputClasses(0) == "*");

        /*
         * Ground Mwir Tracker
         */
        BOOST_TEST_MESSAGE("Starting test - Ground Mwir Tracker");
        BOOST_TEST(startConfiguration.getGroundMwirTracker().getEnable() == true);
        BOOST_TEST(startConfiguration.getGroundMwirTracker().getMaxTimeSinceUpdateToReport() == 5);
        BOOST_TEST(startConfiguration.getGroundMwirTracker().getParameters().getInitTrackSize() == 15);
        BOOST_TEST(startConfiguration.getGroundMwirTracker().getParameters().getInitThreshold() == 0.4f);
        BOOST_TEST(startConfiguration.getGroundMwirTracker().getParameters().getInitMetric() == sdk::InitScoreMetric::Median);
        BOOST_TEST(startConfiguration.getGroundMwirTracker().getParameters().getMaxPredictedFrames() == 25);
        BOOST_TEST(startConfiguration.getGroundMwirTracker().getParameters().getTrackFeaturesHistorySize() == 1);
        BOOST_TEST(startConfiguration.getGroundMwirTracker().getParameters().getFeatureWeight() == 0);
        BOOST_TEST(startConfiguration.getGroundMwirTracker().getParameters().getMaxCosineDistance() == 0.15f);
        BOOST_TEST(startConfiguration.getGroundMwirTracker().getParameters().getMaxMahalanobisDistance() == 6);
        BOOST_TEST(startConfiguration.getGroundMwirTracker().getParameters().getMinIouThreshold() == 0.2f);

        /*
         * Ground RgbSwir Flow ID
         */
        startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
        BOOST_TEST_MESSAGE("Starting test - Ground RgbSwir Flow ID");
        BOOST_TEST(startConfiguration.getFlowSwitcher().getFlowId() == sdk::FlowSwitcherFlowId::GroundRgbAndSwir);

//        /*
//         * Ground Swir Flow ID
//         */
//        startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundSwir);
//        BOOST_TEST_MESSAGE("Starting test - Ground Swir Flow ID");
//        BOOST_TEST(startConfiguration.getFlowSwitcher().getFlowId() == sdk::FlowSwitcherFlowId::GroundSwir);
        /*
         * Ground RgbSwir Detector
         */
        BOOST_TEST_MESSAGE("Starting test - Ground RgbSwir Detector");
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getRoi().getWidth() == 0);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getRoi().getHeight() == 0);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getRoi().getX() == 0);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getRoi().getY() == 0);
        // light-vehicle
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("light-vehicle").getScoreThreshold() == 0.3f);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("light-vehicle").getMinWidth() == 1);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("light-vehicle").getMaxWidth() == 1000);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("light-vehicle").getMinHeight() == 1);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("light-vehicle").getMaxHeight() == 1000);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("light-vehicle").getMinAspectRatio() == 0);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("light-vehicle").getMaxAspectRatio() == 100);
        // person
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("person").getScoreThreshold() == 0.3f);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("person").getMinWidth() == 1);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("person").getMaxWidth() == 1000);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("person").getMinHeight() == 1);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("person").getMaxHeight() == 1000);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("person").getMinAspectRatio() == 0);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("person").getMaxAspectRatio() == 100);
        // two-wheeled
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("two-wheeled").getScoreThreshold() == 0.3f);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("two-wheeled").getMinWidth() == 1);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("two-wheeled").getMaxWidth() == 1000);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("two-wheeled").getMinHeight() == 1);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("two-wheeled").getMaxHeight() == 1000);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("two-wheeled").getMinAspectRatio() == 0);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("two-wheeled").getMaxAspectRatio() == 100);
        /*
        * Ground Swir Detector
        */
//        BOOST_TEST_MESSAGE("Starting test - Ground Swir Detector");
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getRoi().getWidth() == 0);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getRoi().getHeight() == 0);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getRoi().getX() == 0);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getRoi().getY() == 0);
//        // light-vehicle
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("light-vehicle").getScoreThreshold() == 0.4f);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("light-vehicle").getMinWidth() == 1);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("light-vehicle").getMaxWidth() == 1000);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("light-vehicle").getMinHeight() == 1);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("light-vehicle").getMaxHeight() == 1000);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("light-vehicle").getMinAspectRatio() == 0);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("light-vehicle").getMaxAspectRatio() == 100);
//        // person
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("person").getScoreThreshold() == 0.6f);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("person").getMinWidth() == 1);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("person").getMaxWidth() == 1000);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("person").getMinHeight() == 1);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("person").getMaxHeight() == 1000);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("person").getMinAspectRatio() == 0);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("person").getMaxAspectRatio() == 100);
//        // two-wheeled
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("two-wheeled").getScoreThreshold() == 0.4f);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("two-wheeled").getMinWidth() == 1);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("two-wheeled").getMaxWidth() == 1000);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("two-wheeled").getMinHeight() == 1);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("two-wheeled").getMaxHeight() == 1000);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("two-wheeled").getMinAspectRatio() == 0);
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getGroups("two-wheeled").getMaxAspectRatio() == 100);

        /*
         * Ground RgbSwir Postprocessor
         */

        BOOST_TEST_MESSAGE("Starting test - Ground RgbSwir Postprocessor");
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getOutputClasses(0) == "*");
        /*
         * Ground Swir Postprocessor
         * */
//        BOOST_TEST_MESSAGE("Starting test - Ground Swir Postprocessor");
//        BOOST_TEST(startConfiguration.getGroundSwirDetector().getOutputClasses(0) == "*");

        /*
         * Ground RgbSwir Tracker
         */
        BOOST_TEST_MESSAGE("Starting test - Ground RgbSwir Tracker");
        BOOST_TEST(startConfiguration.getGroundRgbSwirTracker().getEnable() == true);
        BOOST_TEST(startConfiguration.getGroundRgbSwirTracker().getMaxTimeSinceUpdateToReport() == 5);
        BOOST_TEST(startConfiguration.getGroundRgbSwirTracker().getParameters().getInitTrackSize() == 15);
        BOOST_TEST(startConfiguration.getGroundRgbSwirTracker().getParameters().getInitThreshold() == 0.4f);
        BOOST_TEST(startConfiguration.getGroundRgbSwirTracker().getParameters().getInitMetric() == sdk::InitScoreMetric::Median);
        BOOST_TEST(startConfiguration.getGroundRgbSwirTracker().getParameters().getMaxPredictedFrames() == 30);
        BOOST_TEST(startConfiguration.getGroundRgbSwirTracker().getParameters().getTrackFeaturesHistorySize() == 1);
        BOOST_TEST(startConfiguration.getGroundRgbSwirTracker().getParameters().getFeatureWeight() == 0);
        BOOST_TEST(startConfiguration.getGroundRgbSwirTracker().getParameters().getMaxCosineDistance() == 0.15f);
        BOOST_TEST(startConfiguration.getGroundRgbSwirTracker().getParameters().getMaxMahalanobisDistance() == 5);
        BOOST_TEST(startConfiguration.getGroundRgbSwirTracker().getParameters().getMinIouThreshold() == 0.2f);
        /*
        * Ground Swir Tracker
        */
//        BOOST_TEST_MESSAGE("Starting test - Ground Swir Tracker");
//        BOOST_TEST(startConfiguration.getGroundSwirTracker().getEnable() == true);
//        BOOST_TEST(startConfiguration.getGroundSwirTracker().getMaxTimeSinceUpdateToReport() == 5);
//        BOOST_TEST(startConfiguration.getGroundSwirTracker().getParameters().getInitTrackSize() == 15);
//        BOOST_TEST(startConfiguration.getGroundSwirTracker().getParameters().getInitThreshold() == 0.4f);
//        BOOST_TEST(startConfiguration.getGroundSwirTracker().getParameters().getInitMetric() == sdk::InitScoreMetric::Median);
//        BOOST_TEST(startConfiguration.getGroundSwirTracker().getParameters().getMaxPredictedFrames() == 30);
//        BOOST_TEST(startConfiguration.getGroundSwirTracker().getParameters().getTrackFeaturesHistorySize() == 1);
//        BOOST_TEST(startConfiguration.getGroundSwirTracker().getParameters().getFeatureWeight() == 0);
//        BOOST_TEST(startConfiguration.getGroundSwirTracker().getParameters().getMaxCosineDistance() == 0.15f);
//        BOOST_TEST(startConfiguration.getGroundSwirTracker().getParameters().getMaxMahalanobisDistance() == 5);
//        BOOST_TEST(startConfiguration.getGroundSwirTracker().getParameters().getMinIouThreshold() == 0.2f);
        /*
         * Tracks Publisher
         */
        BOOST_TEST_MESSAGE("Starting test - Tracks Publisher");
        BOOST_TEST(startConfiguration.getTracksPublisher().getSourceData() == sdk::PublisherDataType::Tracks);

        /*
         * Renderer
         */
        BOOST_TEST_MESSAGE("Starting test - Renderer");
        BOOST_TEST(startConfiguration.getRenderer().getTargetWidth() == 1280);
        BOOST_TEST(startConfiguration.getRenderer().getTargetHeight() == 720);
        BOOST_TEST(startConfiguration.getRenderer().getSkipAllRendering() == false);
        BOOST_TEST(startConfiguration.getRenderer().getNormalization().getType() == sdk::NormalizationType::MeanStdCrop);
        BOOST_TEST(startConfiguration.getRenderer().getNormalization().getAlpha() == 1);
        BOOST_TEST(startConfiguration.getRenderer().getNormalization().getBeta() == 0);
        BOOST_TEST(startConfiguration.getRenderer().getNormalization().getStdCropSize() == 2);
        BOOST_TEST(startConfiguration.getRenderer().getDrawDetections() == false);
        BOOST_TEST(startConfiguration.getRenderer().getDrawTracks() == true);
        BOOST_TEST(startConfiguration.getRenderer().getPrintClass() == true);
        BOOST_TEST(startConfiguration.getRenderer().getPrintScore() == false);
        BOOST_TEST(startConfiguration.getRenderer().getPrintTrackId() == true);
        BOOST_TEST(startConfiguration.getRenderer().getColorTrack() == true);
        BOOST_TEST(startConfiguration.getRenderer().getDrawTrackVelocity() == true);
        BOOST_TEST(startConfiguration.getRenderer().getTrackVelocityFactor() == 1);
        BOOST_TEST(startConfiguration.getRenderer().getPointTrackRadius() == 10);
        //LIOR CHANGE V1.2
        BOOST_TEST(startConfiguration.getRenderer().getPrintDistance() == false);
        BOOST_TEST(startConfiguration.getRenderer().getDrawCenterOfMass() == false);
        BOOST_TEST(startConfiguration.getRenderer().getCenterOfMassRadius() == 3);

        // Detector ROI
        BOOST_TEST(startConfiguration.getRenderer().getDetectorRoi().getSkipRendering() == false);
        BOOST_TEST(startConfiguration.getRenderer().getDetectorRoi().getColor() == 16776960);
        BOOST_TEST(startConfiguration.getRenderer().getDetectorRoi().getLineThickness() == 1);
        BOOST_TEST(startConfiguration.getRenderer().getDetectorRoi().getHideOutside() == false);
//        BOOST_TEST(startConfiguration.getRenderer().getDetectorRoi().getOutsideColor() == 0);
        BOOST_TEST_MESSAGE("Testing BB");
        // Bounding Boxes
        BOOST_TEST(startConfiguration.getRenderer().getBoundingBoxes("").getSkipRendering() == false);
        BOOST_TEST(startConfiguration.getRenderer().getBoundingBoxes("").getBoxColor() == 16711680);
        BOOST_TEST(startConfiguration.getRenderer().getBoundingBoxes("").getLineThickness() == 1);
        BOOST_TEST(startConfiguration.getRenderer().getBoundingBoxes("").getTextOffsetX() == 0);
        BOOST_TEST(startConfiguration.getRenderer().getBoundingBoxes("").getTextOffsetY() == -8);
        BOOST_TEST(startConfiguration.getRenderer().getBoundingBoxes("").getFontColor() == 0);
        BOOST_TEST(startConfiguration.getRenderer().getBoundingBoxes("").getFontScale() == 0.5f);
        BOOST_TEST(startConfiguration.getRenderer().getBoundingBoxes("").getFontThickness() == 1);
        // OSD
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getSkipRendering() == false);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getCorner() == sdk::Corner::TopLeft);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getValues(0) == sdk::OsdValue::Version);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getValues(1) == sdk::OsdValue::StreamId);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getValues(2) == sdk::OsdValue::InputDescription);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getValues(3) == sdk::OsdValue::FrameId);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getValues(4) == sdk::OsdValue::Pts);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getValues(5) == sdk::OsdValue::Fps);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getValues(6) == sdk::OsdValue::Latency);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getValues(7) == sdk::OsdValue::Telemetry);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getValues(8) == sdk::OsdValue::FrameRegistration);
//        BOOST_TEST(startConfiguration.getRenderer().getOsd().getValues(9) == sdk::OsdValue::Tracks);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getMarginX() == 10);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getMarginY() == 10);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getLineDistance() == 10);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getBackColor() == 0);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getBackTransparency() == 0.7f);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getFontColor() == 16711935);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getFontScale() == 0.5f);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getFontThickness() == 1);
        // Histogram
        BOOST_TEST(startConfiguration.getRenderer().getHistogram().getSkipRendering() == true);
        BOOST_TEST(startConfiguration.getRenderer().getHistogram().getCorner() == sdk::Corner::BottomLeft);
        BOOST_TEST(startConfiguration.getRenderer().getHistogram().getMarginX() == 10);
        BOOST_TEST(startConfiguration.getRenderer().getHistogram().getMarginY() == 10);
        BOOST_TEST(startConfiguration.getRenderer().getHistogram().getBins() == 100);
        BOOST_TEST(startConfiguration.getRenderer().getHistogram().getWidth() == 300);
        BOOST_TEST(startConfiguration.getRenderer().getHistogram().getHeight() == 200);
        BOOST_TEST(startConfiguration.getRenderer().getHistogram().getBackColor() == 0);
        BOOST_TEST(startConfiguration.getRenderer().getHistogram().getBackTransparency() == 0.7f);
        BOOST_TEST(startConfiguration.getRenderer().getHistogram().getColorBefore() == 16776960);
        BOOST_TEST(startConfiguration.getRenderer().getHistogram().getColorAfter() == 65280);

        /*
         * Preview Module
         */
        BOOST_TEST_MESSAGE("Starting test - Preview Module");
        BOOST_TEST(startConfiguration.getPreviewModule().getEnable() == false);

        /*
         * GstSink
         */
        BOOST_TEST_MESSAGE("Starting test - GstSink");
        BOOST_TEST(startConfiguration.getGstSink().getSink().toString() == "rtspclientsink location={}");
        BOOST_TEST(startConfiguration.getGstSink().getUrl().toString() == sinkStr);
        BOOST_TEST(startConfiguration.getGstSink().getFrameRate() == 0);

        // V1.2 NEW TESTS FOR ICD (default values)
        /*
         * DebugModule
         */
        BOOST_TEST_MESSAGE("Starting test - DebugModule");
//        BOOST_TEST(startConfiguration.getDebugModule().getEnable() == false);

        BOOST_TEST_MESSAGE("Starting test - FramerateController configuration");
        BOOST_TEST(startConfiguration.getFramerateController().getOutputFramerate() == 0);

        BOOST_TEST_MESSAGE("Starting test - seaMwirTrackerRate configuration");
        BOOST_TEST(startConfiguration.getSeaMwirTrackerRate().getOutputFramerate() == 0);

        BOOST_TEST_MESSAGE("Starting test - seaSwirTrackerRate configuration");
        BOOST_TEST(startConfiguration.getSeaSwirTrackerRate().getOutputFramerate() == 0);

        BOOST_TEST_MESSAGE("Starting test - GroundMwirTrackerRate configuration");
        BOOST_TEST(startConfiguration.getGroundMwirTrackerRate().getOutputFramerate() == 0);

        BOOST_TEST_MESSAGE("Starting test - GroundRgbSwirTrackerRate configuration");
        BOOST_TEST(startConfiguration.getGroundRgbSwirTrackerRate().getOutputFramerate() == 0);

//        BOOST_TEST_MESSAGE("Starting test - GroundSwirTrackerRate configuration");
//        BOOST_TEST(startConfiguration.getGroundSwirTrackerRate().getOutputFramerate() == 0);

        BOOST_TEST_MESSAGE("Starting test - RangeEstimator");
        BOOST_TEST(startConfiguration.getRangeEstimator().getClasses(0).getClassName() == "person");
        BOOST_TEST(startConfiguration.getRangeEstimator().getClasses(0).getWidthInMeters() == 0);
        BOOST_TEST(startConfiguration.getRangeEstimator().getClasses(0).getHeightInMeters() == 1.8f);
//        BOOST_TEST(startConfiguration.getRangeEstimator().getUseLandmarks() == true);
        BOOST_TEST(startConfiguration.getRangeEstimator().getSmoothingFactor() == 0.99f);

}
BOOST_AUTO_TEST_SUITE_END() // NOLINT icd_tests


BOOST_AUTO_TEST_SUITE(icd_tests1) // NOLINT
BOOST_AUTO_TEST_SUITE(preprocessor)
BOOST_AUTO_TEST_CASE(rotate_angle){
    BOOST_TEST_MESSAGE("Starting preprocessor rotate angle test...");
    rotateAngleTestFunc(sdk::FlowSwitcherFlowId::Unspecified);
}
BOOST_AUTO_TEST_CASE(roi){
    BOOST_TEST_MESSAGE("Starting preprocessor ROI test...");
    roiTestFunc<typeof(sdk::PreprocessorStartStreamConfiguration), size_t>(sdk::FlowSwitcherFlowId::Unspecified);
}
BOOST_AUTO_TEST_SUITE_END() // NOLINT preprocessor


BOOST_AUTO_TEST_CASE(flow_switcher) { // NOLINT
        //TODO - decide if create also flow_switcher_test_func
        try {

            BOOST_TEST_MESSAGE("Starting flow switcher test...");
            /**
             * Update test
             */
            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
            waitForStreamStarted(stream);
            sendFrames(stream, 100, nullptr);
            sendFrames(stream, 50,
                       [&stream](uint32_t nFrameId) {
                           if (nFrameId == 10) {
                               BOOST_TEST_MESSAGE(stream->getConfiguration().getFlowSwitcher().getFlowId());
                               stream->getConfiguration().getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundMwir);
                               stream->update();
                               BOOST_TEST(stream->getConfiguration().getFlowSwitcher().getFlowId() == sdk::FlowSwitcherFlowId::GroundMwir);
                           } else if (nFrameId == 20) {
                               BOOST_TEST_MESSAGE(stream->getConfiguration().getFlowSwitcher().getFlowId());
                               stream->getConfiguration().getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
                               stream->update();
                               BOOST_TEST(stream->getConfiguration().getFlowSwitcher().getFlowId() == sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
                           } else if (nFrameId == 30) {
                               BOOST_TEST_MESSAGE(stream->getConfiguration().getFlowSwitcher().getFlowId());
                               stream->getConfiguration().getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::SeaSwir);
                               stream->update();
                               BOOST_TEST(stream->getConfiguration().getFlowSwitcher().getFlowId() == sdk::FlowSwitcherFlowId::SeaSwir);
                           } else if (nFrameId == 40) {
                               BOOST_TEST_MESSAGE(stream->getConfiguration().getFlowSwitcher().getFlowId());
                               stream->getConfiguration().getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::SeaMwir);
                               stream->update();
                               BOOST_TEST(stream->getConfiguration().getFlowSwitcher().getFlowId() == sdk::FlowSwitcherFlowId::SeaMwir);
                           }
                       }
            );
            std::this_thread::sleep_for(std::chrono::milliseconds(3000));
            stream.reset();
        }
        catch (sdk::Exception& e) {
            BOOST_TEST((((std::string) e.what()).find("not registered") != std::string::npos || ((std::string) e.what()).find("Deadline Exceeded") != std::string::npos),
                       "Caught unexpected error: " + (std::string) e.what());
            BOOST_TEST_MESSAGE((std::string) e.what());
        }
}

BOOST_AUTO_TEST_SUITE(detector) // NOLINT
BOOST_AUTO_TEST_SUITE(detector_roi_start)   // NOLINT
BOOST_AUTO_TEST_CASE(sea_mwir){
    BOOST_TEST_MESSAGE("Starting detector ROI test...");
    a_strVideoPath = seaMwirVideo;
    roiTestFunc<typeof(sdk::SeaMwirDetectorStartStreamConfiguration), int>(sdk::FlowSwitcherFlowId::SeaMwir);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

BOOST_AUTO_TEST_CASE(ground_mwir){
    a_strVideoPath = groundMwirVideo;
    roiTestFunc<typeof(sdk::GroundMwirDetectorStartStreamConfiguration), float>(sdk::FlowSwitcherFlowId::GroundMwir);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

BOOST_AUTO_TEST_CASE(ground_rgb_swir){
    a_strVideoPath = groundRgbVideo;
    roiTestFunc<typeof(sdk::GroundRgbSwirDetectorStartStreamConfiguration), double>(sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}

//BOOST_AUTO_TEST_CASE(ground_swir){
//    a_strVideoPath = groundSwirVideo;
//    roiTestFunc<typeof(sdk::GroundSwirDetectorStartStreamConfiguration), long>(sdk::FlowSwitcherFlowId::GroundSwir);
//    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
//}

BOOST_AUTO_TEST_CASE(sea_swir){
    a_strVideoPath = seaSwirVideo;
    roiTestFunc<typeof(sdk::SeaSwirDetectorStartStreamConfiguration), uint32_t>(sdk::FlowSwitcherFlowId::SeaSwir);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
}
BOOST_AUTO_TEST_SUITE_END() // NOLINT detector_roi_start


BOOST_AUTO_TEST_SUITE(detector_roi_update) // NOLINT
BOOST_AUTO_TEST_CASE(sea_mwir){
    a_strVideoPath = seaMwirVideo;
    updateRoiTestFunc<typeof(sdk::SeaMwirDetectorUpdateStreamConfiguration), int>(sdk::FlowSwitcherFlowId::SeaMwir);
}

BOOST_AUTO_TEST_CASE(ground_mwir){
    a_strVideoPath = groundMwirVideo;
    updateRoiTestFunc<typeof(sdk::GroundMwirDetectorUpdateStreamConfiguration), float>(sdk::FlowSwitcherFlowId::GroundMwir);
}

BOOST_AUTO_TEST_CASE(ground_rgb_swir) {
    a_strVideoPath = groundRgbVideo;
    updateRoiTestFunc<typeof(sdk::GroundRgbSwirDetectorUpdateStreamConfiguration), double>(sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
}

//BOOST_AUTO_TEST_CASE(ground_swir){
//    a_strVideoPath = groundSwirVideo;
//    updateRoiTestFunc<typeof(sdk::GroundSwirDetectorUpdateStreamConfiguration), long>(sdk::FlowSwitcherFlowId::GroundSwir);
//}

BOOST_AUTO_TEST_CASE(sea_swir){
    a_strVideoPath = seaSwirVideo;
    updateRoiTestFunc<typeof(sdk::SeaSwirDetectorUpdateStreamConfiguration), uint32_t>(sdk::FlowSwitcherFlowId::SeaSwir);
}
BOOST_AUTO_TEST_SUITE_END() // NOLINT detector_roi_update

BOOST_AUTO_TEST_SUITE(detector_groups) // NOLINT
BOOST_AUTO_TEST_CASE(sea_mwir){
    BOOST_TEST_MESSAGE("Starting detector groups tests...");
    a_strVideoPath = seaMwirVideo;
    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
    thresholdTestFunc<typeof(startConfiguration.getSeaMwirDetector()), int>(seaGroups, sdk::FlowSwitcherFlowId::SeaMwir);
    minMaxTestFunc<typeof(startConfiguration.getSeaMwirDetector()), int>(seaGroups, sdk::FlowSwitcherFlowId::SeaMwir);
}
BOOST_AUTO_TEST_CASE(ground_mwir){
    a_strVideoPath = groundMwirVideo;
    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
    thresholdTestFunc<typeof(startConfiguration.getGroundMwirDetector()), float>(groundGroups, sdk::FlowSwitcherFlowId::GroundMwir);
    minMaxTestFunc<typeof(startConfiguration.getGroundMwirDetector()), float>(groundGroups, sdk::FlowSwitcherFlowId::GroundMwir);
}

BOOST_AUTO_TEST_CASE(ground_rgb_swir){
    a_strVideoPath = groundRgbVideo;
    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
    thresholdTestFunc<typeof(startConfiguration.getGroundRgbSwirDetector()), double>(groundGroups, sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
    minMaxTestFunc<typeof(startConfiguration.getGroundRgbSwirDetector()), double>(groundGroups, sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
}
//BOOST_AUTO_TEST_CASE(ground_swir){
//    a_strVideoPath = groundSwirVideo;
//    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
//    thresholdTestFunc<typeof(startConfiguration.getGroundSwirDetector()), long>(groundGroups, sdk::FlowSwitcherFlowId::GroundSwir);
//    minMaxTestFunc<typeof(startConfiguration.getGroundSwirDetector()), long>(groundGroups, sdk::FlowSwitcherFlowId::GroundSwir);
//}
BOOST_AUTO_TEST_CASE(sea_swir){
    a_strVideoPath = seaSwirVideo;
    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
    thresholdTestFunc<typeof(startConfiguration.getSeaSwirDetector()), uint32_t>(seaGroups, sdk::FlowSwitcherFlowId::SeaSwir);
    minMaxTestFunc<typeof(startConfiguration.getSeaSwirDetector()), uint32_t>(seaGroups, sdk::FlowSwitcherFlowId::SeaSwir);
}

BOOST_AUTO_TEST_SUITE_END() // NOLINT detector_groups
BOOST_AUTO_TEST_SUITE_END() // NOLINT detector_configuration

BOOST_AUTO_TEST_SUITE(tracker)
BOOST_AUTO_TEST_SUITE(tracker_rate)
BOOST_AUTO_TEST_CASE(sea_mwir){
    a_strVideoPath = seaMwirVideo;
    trackerRateTestFunc<typeof(sdk::SeaMwirTrackerRateStartStreamConfiguration), int>(sdk::FlowSwitcherFlowId::SeaMwir);
}
BOOST_AUTO_TEST_CASE(ground_mwir){
    a_strVideoPath = groundMwirVideo;
    trackerRateTestFunc<typeof(sdk::GroundMwirTrackerRateStartStreamConfiguration), float>(sdk::FlowSwitcherFlowId::GroundMwir);
}
BOOST_AUTO_TEST_CASE(sea_swir){
    a_strVideoPath = seaSwirVideo;
    trackerRateTestFunc<typeof(sdk::SeaSwirTrackerRateStartStreamConfiguration), uint32_t>(sdk::FlowSwitcherFlowId::SeaSwir);
}
BOOST_AUTO_TEST_CASE(ground_rgb_swir){
    a_strVideoPath = groundRgbVideo;
    trackerRateTestFunc<typeof(sdk::GroundRgbSwirTrackerRateStartStreamConfiguration), double>(sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
}
//BOOST_AUTO_TEST_CASE(ground_swir){
//    a_strVideoPath = groundSwirVideo;
//    trackerRateTestFunc<typeof(sdk::GroundSwirTrackerRateStartStreamConfiguration), long>(sdk::FlowSwitcherFlowId::GroundSwir);
//}
BOOST_AUTO_TEST_SUITE_END() // NOLINT tracker_rate

BOOST_AUTO_TEST_SUITE(tracker_configuration)
BOOST_AUTO_TEST_CASE(sea_mwir){
    a_strVideoPath = seaMwirVideo;
    trackerTestFunc<typeof(sdk::SeaMwirTrackerStartStreamConfiguration), int>(sdk::FlowSwitcherFlowId::SeaMwir);
}
BOOST_AUTO_TEST_CASE(ground_mwir){
    a_strVideoPath = groundMwirVideo;
    trackerTestFunc<typeof(sdk::GroundMwirTrackerStartStreamConfiguration), float>(sdk::FlowSwitcherFlowId::GroundMwir);
}
BOOST_AUTO_TEST_CASE(sea_swir){
    a_strVideoPath = seaSwirVideo;
    trackerTestFunc<typeof(sdk::SeaSwirTrackerStartStreamConfiguration), uint32_t>(sdk::FlowSwitcherFlowId::SeaSwir);
}
BOOST_AUTO_TEST_CASE(ground_rgb_swir){
   a_strVideoPath = groundRgbVideo;
   trackerTestFunc<typeof(sdk::GroundRgbSwirTrackerStartStreamConfiguration), double>(sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
}
//BOOST_AUTO_TEST_CASE(ground_swir){
//    a_strVideoPath = groundSwirVideo;
//    trackerTestFunc<typeof(sdk::GroundSwirTrackerStartStreamConfiguration), long>(sdk::FlowSwitcherFlowId::GroundSwir);
//}
BOOST_AUTO_TEST_SUITE_END() // NOLINT tracker_configuration
BOOST_AUTO_TEST_SUITE_END() // NOLINT tracker

//    BOOST_AUTO_TEST_SUITE(postprocessor) // NOLINT
//        BOOST_AUTO_TEST_CASE(postprocessor_configuration) {  // NOLINT
//            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
//
//            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
//
//        }
//    BOOST_AUTO_TEST_SUITE_END() // NOLINT

//BOOST_AUTO_TEST_SUITE_END() // NOLINT

//TODO - rangeEstimation and TracksPublisher not implemented
//BOOST_AUTO_TEST_SUITE(range_estimator)
//BOOST_AUTO_TEST_SUITE_END() // NOLINT range_estimator


BOOST_AUTO_TEST_SUITE(renderer) // NOLINT
BOOST_AUTO_TEST_CASE(renderer_general) { // NOLINT
        try {
            a_strVideoPath = groundMwirVideo;
            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
            startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundMwir);
            BOOST_TEST_MESSAGE("Starting test - Renderer General");
            startConfiguration.getRenderer().setSkipAllRendering(true);
            startConfiguration.getRenderer().setDrawDetections(true);
            startConfiguration.getRenderer().setDrawTracks(false);
            startConfiguration.getRenderer().setPrintClass(false);
            startConfiguration.getRenderer().setPrintScore(true);
            startConfiguration.getRenderer().setPrintTrackId(false);
            startConfiguration.getRenderer().setColorTrack(false);
            startConfiguration.getRenderer().setDrawTrackVelocity(false);
            startConfiguration.getRenderer().setTrackVelocityFactor(100);

            BOOST_TEST(startConfiguration.getRenderer().getSkipAllRendering() == true);
            BOOST_TEST(startConfiguration.getRenderer().getDrawDetections() == true);
            BOOST_TEST(startConfiguration.getRenderer().getDrawTracks() == false);
            BOOST_TEST(startConfiguration.getRenderer().getPrintClass() == false);
            BOOST_TEST(startConfiguration.getRenderer().getPrintScore() == true);
            BOOST_TEST(startConfiguration.getRenderer().getPrintTrackId() == false);
            BOOST_TEST(startConfiguration.getRenderer().getColorTrack() == false);
            BOOST_TEST(startConfiguration.getRenderer().getDrawTrackVelocity() == false);
            BOOST_TEST(startConfiguration.getRenderer().getTrackVelocityFactor() == 100);

            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);

            waitForStreamStarted(stream);
            rendererTestFunc();
            sendFrames(stream, 500, nullptr);
            sendFrames(stream, 200,
                       [&stream](uint32_t nFrameId) {
                           if (nFrameId == 10) {
                               //                   BOOST_TEST_MESSAGE("Frame ID: " + std::to_string(nFrameId));
                               stream->getConfiguration().getRenderer().setSkipAllRendering(false);
                               stream->update();
                               BOOST_TEST(stream->getConfiguration().getRenderer().getSkipAllRendering() == false);
                           } else if (nFrameId == 20) {
                               //                   BOOST_TEST_MESSAGE("Frame ID: " + std::to_string(nFrameId));
                               stream->getConfiguration().getRenderer().setDrawDetections(false);
                               stream->update();
                               BOOST_TEST(stream->getConfiguration().getRenderer().getDrawDetections() == false);
                           } else if (nFrameId == 30) {
                               //                   BOOST_TEST_MESSAGE("Frame ID: " + std::to_string(nFrameId));
                               stream->getConfiguration().getRenderer().setDrawTracks(true);
                               stream->update();
                               BOOST_TEST(stream->getConfiguration().getRenderer().getDrawTracks() == true);
                           } else if (nFrameId == 40) {
                               //                   BOOST_TEST_MESSAGE("Frame ID: " + std::to_string(nFrameId));
                               stream->getConfiguration().getRenderer().setPrintClass(true);
                               stream->update();
                               BOOST_TEST(stream->getConfiguration().getRenderer().getPrintClass() == true);
                           } else if (nFrameId == 50) {
                               //                   BOOST_TEST_MESSAGE("Frame ID: " + std::to_string(nFrameId));
                               stream->getConfiguration().getRenderer().setPrintScore(false);
                               stream->update();
                               BOOST_TEST(stream->getConfiguration().getRenderer().getPrintScore() == false);
                           } else if (nFrameId == 60) {
                               //                   BOOST_TEST_MESSAGE("Frame ID: " + std::to_string(nFrameId));
                               stream->getConfiguration().getRenderer().setPrintTrackId(true);
                               stream->update();
                               BOOST_TEST(stream->getConfiguration().getRenderer().getPrintTrackId() == true);
                           } else if (nFrameId == 70) {
                               //                   BOOST_TEST_MESSAGE("Frame ID: " + std::to_string(nFrameId));
                               stream->getConfiguration().getRenderer().setColorTrack(true);
                               stream->update();
                               BOOST_TEST(stream->getConfiguration().getRenderer().getColorTrack() == true);
                           } else if (nFrameId == 80) {
                               //                   BOOST_TEST_MESSAGE("Frame ID: " + std::to_string(nFrameId));
                               stream->getConfiguration().getRenderer().setDrawTrackVelocity(true);
                               stream->update();
                               BOOST_TEST(stream->getConfiguration().getRenderer().getDrawTrackVelocity() == true);
                           } else if (nFrameId == 90) {
                               //                   BOOST_TEST_MESSAGE("Frame ID: " + std::to_string(nFrameId));
                               stream->getConfiguration().getRenderer().setTrackVelocityFactor(35.5f);
                               stream->update();
                               BOOST_TEST(stream->getConfiguration().getRenderer().getTrackVelocityFactor() == 35.5);
                           }
                       }
            );

            BOOST_TEST_MESSAGE("Test finished");
            stream.reset();
        }
        catch (sdk::Exception& e) {
            BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos, "Caught unexpected error: " + (std::string) e.what());
        }
}
BOOST_AUTO_TEST_SUITE_END() // NOLINT renderer
BOOST_AUTO_TEST_SUITE_END() // NOLINT icd_tests1

BOOST_AUTO_TEST_SUITE(output) //NOLINT
BOOST_AUTO_TEST_SUITE(full_video) //NOLINT
BOOST_AUTO_TEST_CASE(sea_mwir){
    BOOST_TEST_MESSAGE("Running full videos");
    BOOST_TEST_MESSAGE("Sea MWIR");
    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
    a_strVideoPath = seaMwirVideo;
    std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
    waitForStreamStarted(stream);
    sendFrames(stream, -1, nullptr);
}

BOOST_AUTO_TEST_CASE(ground_mwir){
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    BOOST_TEST_MESSAGE("Ground MWIR");
    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
    startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundMwir);
    a_strVideoPath = groundMwirVideo;
    std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
    waitForStreamStarted(stream);
    sendFrames(stream, -1, nullptr);
}
BOOST_AUTO_TEST_CASE(sea_swir){
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    BOOST_TEST_MESSAGE("Sea SWIR");
    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
    startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::SeaSwir);
    a_strVideoPath = seaSwirVideo;
    std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
    waitForStreamStarted(stream);
    sendFrames(stream, -1, nullptr);
}
BOOST_AUTO_TEST_CASE(ground_rgb_swir){
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    BOOST_TEST_MESSAGE("Ground RGB");
    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
    startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
    a_strVideoPath = groundRgbVideo;
    std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
    waitForStreamStarted(stream);
    sendFrames(stream, -1, nullptr);
}

//BOOST_AUTO_TEST_CASE(ground_swir){
//    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//    BOOST_TEST_MESSAGE("Ground SWIR");
//    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
//    a_strVideoPath = groundSwirVideo;
//    std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
//    waitForStreamStarted(stream);
//    sendFrames(stream, -1, nullptr);
//}
//    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//    stream.reset();

BOOST_AUTO_TEST_CASE(output_preview) { //NOLINT
    try {

        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getPreviewModule().setEnable(true);
        currentStreamError = streamError::PreviewError;
        errorCounter++;
        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
    }
    catch (const sdk::Exception &e) {
        std::cerr << "Error is: " + (std::string) e.what() << std::endl;
    }
    BOOST_TEST(errorCounter == 0, "Not all expected errors returned! Number of missed errors: " + std::to_string(errorCounter));
}
BOOST_AUTO_TEST_SUITE_END() // NOLINT full_video
BOOST_AUTO_TEST_SUITE_END() // NOLINT output

BOOST_AUTO_TEST_SUITE(debug) // NOLINT
    BOOST_AUTO_TEST_CASE(debug_environment) { // NOLINT
        std::string settingsFile = "/home/lior.lakay/Desktop/sdk_runner/stream_settings.bin";
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration(settingsFile);
        a_strVideoPath = groundMwirVideo;
        BOOST_TEST(startConfiguration.getFlowSwitcher().getFlowId() == sdk::FlowSwitcherFlowId::GroundMwir);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("light-vehicle").getScoreThreshold() == 0.1f);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getSkipRendering() == true);

        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);

        waitForStreamStarted(stream);
        BOOST_TEST_MESSAGE("Check that SightX logo is in the bottom left corner");
        sendFrames(stream, -1, [&stream](uint32_t nFrameId){
            if(nFrameId == 250)
                BOOST_TEST(stream->getConfiguration().getRenderer().getOsd().getSkipRendering() == true);
        });
    }
BOOST_AUTO_TEST_SUITE_END() // NOLINT debug

//BOOST_AUTO_TEST_CASE(test1){
//
//    std::vector<std::string> videos = {
//            "/home/lior.lakay/Desktop/sdk_runner/test_videos/Ground_MWIR/groundMWIR1",
//            "home/tom/Desktop/sdk_runner/test_videos/Ground_MWIR/groundMWIR2"
//    };
//    for (std::string video : videos) {
//        a_strVideoPath = video;
//        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
//        startConfiguration.getRenderer().getOsd().setSkipRendering(true);
////        startConfiguration.getDebugModule();
////        startConfiguration.getDebugModule().setEnable(true);
//        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
//        waitForStreamStarted(stream);
//
//        sendFrames(stream, 1000, nullptr);
//
//
//    }
//}

//BOOST_AUTO_TEST_CASE(test){
//    try {
//        a_strVideoPath = groundMwirVideo;
//        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
//        startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundMwir);
//        startConfiguration.getGroundMwirDetector().getRoi().setWidth(700);
////        startConfiguration.getGroundMwirDetector().getRoi().setHeight(700);
//        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
//        waitForStreamStarted(stream);
////        std::cout << stream->getConfiguration().getGroundMwirDetector().getRoi().getWidth() << std::endl;
//        sendFrames(stream, 1000, [&stream](int nFrameId){
//            if(nFrameId == 70)
//                std::cout << stream->getConfiguration().getGroundMwirDetector().getRoi().getWidth() << std::endl;
//        });
////        std::cout << stream->getConfiguration().getGroundMwirDetector().getRoi().getWidth() << std::endl;
//    }
//    catch(const sdk::Exception& e){
//        std::cerr << e.what() << std::endl;
//    }
//}

//BOOST_AUTO_TEST_CASE(test2){
//    try {
//        a_strVideoPath = groundMwirVideo;
//        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
//        startConfiguration.getPreprocessor().getRoi().setWidth(641);
////        startConfiguration.getPreprocessor().getRoi().setHeight(1);
//        startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundMwir);
////        startConfiguration.getGroundMwirDetector().getRoi().setWidth(700);
////        startConfiguration.getGroundMwirDetector().getRoi().setHeight(700);
//        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
//        waitForStreamStarted(stream);
////        std::cout << stream->getConfiguration().getGroundMwirDetector().getRoi().getWidth() << std::endl;
//        sendFrames(stream, 1000, [&stream](int nFrameId){
//            if(nFrameId == 70)
//                BOOST_TEST_MESSAGE(stream->getConfiguration().getGroundMwirDetector().getRoi().getWidth());
//        });
////        std::cout << stream->getConfiguration().getGroundMwirDetector().getRoi().getWidth() << std::endl;
//    }
//    catch(const sdk::Exception& e){
//        std::cerr << e.what() << std::endl;
//    }
//}

//BOOST_AUTO_TEST_CASE(test3){
//    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
////    startConfiguration.getGroundMwirDetector().getGroups("person").setScoreThreshold(0.75f);
////    startConfiguration.getDebugModule().setEnable(true);
//    a_strVideoPath = groundRgbVideo;
//    startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
//    std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
////    std::cout << stream->getId() << std::endl;
//    waitForStreamStarted(stream);
//
//    sendFrames(stream, -1, [&stream](uint32_t nFrameId){
//        if(nFrameId % 100 == 0)
//            std::cout << (stream->getStreamInfo().LatencyMs) / 1000 << std::endl;
//    });
//}

BOOST_AUTO_TEST_CASE(test_arm){
    try {
        sdk::StartStreamConfiguration startConf = getStartConfiguration();
        startConf.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
        groundRgbVideo = "/home/lior.lakay/Desktop/sdk_runner/test_videos/Armed/DJI_20221026160239_0001_W.mp4";
        isArmTest = true;
        a_strVideoPath = groundRgbVideo;
        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConf);
        waitForStreamStarted(stream);
        sendFrames(stream, -1, nullptr);
    }
    catch(sdk::Exception& e){
        std::cerr << "Exception= " + (std::string)e.what() << std::endl;
    }
}

BOOST_AUTO_TEST_CASE(testing_pipester){
    try {
        sdk::StartStreamConfiguration startConf = getStartConfiguration();
        startConf.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
        groundRgbVideo = "/home/lior.lakay/Desktop/sdk_runner/test_videos/Ground_RGB/ground_rgb_test_set.avi";
//        isArmTest = true;
        a_strVideoPath = groundRgbVideo;
        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConf);
        waitForStreamStarted(stream);
        sendFrames(stream, -1, nullptr);
    }
    catch(sdk::Exception& e){
        std::cerr << "Exception= " + (std::string)e.what() << std::endl;
    }
}
BOOST_AUTO_TEST_CASE(destroy) { // NOLINT
        mainPipeline.reset();
}


