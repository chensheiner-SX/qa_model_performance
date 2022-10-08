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
    RoiNotInRange,
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
std::map <sdk::FlowSwitcherFlowId, std::string> printFlow = {
        {sdk::FlowSwitcherFlowId::SeaMwir, "*-*-*-*-*-*-*-* Sea Mwir Detector *-*-*-*-*-*-*-*"},
        {sdk::FlowSwitcherFlowId::SeaSwir, "*-*-*-*-*-*-*-* Sea Swir Detector *-*-*-*-*-*-*-*"},
        {sdk::FlowSwitcherFlowId::GroundRgbAndSwir, "*-*-*-*-*-*-*-* Ground Rgb and Swir Detector *-*-*-*-*-*-*-*"},
        {sdk::FlowSwitcherFlowId::GroundMwir, "*-*-*-*-*-*-*-* Ground Mwir Detector *-*-*-*-*-*-*-*"},
        {sdk::FlowSwitcherFlowId::Unspecified, "*-*-*-*-*-*-*-* Preprocessor *-*-*-*-*-*-*-*"}
};

UserData data;
streamError currentStreamError = streamError::NONE;
rangeError currentRangeError = rangeError::None;
FrameDimensions currentFrameDimensions;
Roi currentRoi;
std::shared_ptr <sdk::Pipeline> mainPipeline;
std::shared_ptr <sdk::Stream> currentStream;
std::map<std::string, bool> runningStreams;
cv::VideoCapture videoCapture;

//std::string a_strVideoPath = "rtsp://root:password@172.12.10.199/axis-media/media.amp";
std::string a_strVideoPath;
std::string a_strServerIP;
std::string sinkStr;
cv::Mat matFrame;
uint32_t numOfFrames = 500;
int errorCounter = 0;
bool isRoiTest = false;

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

//template<typename T, typename S>
//std::pair<T, S> getPair(T first, S second) {
//    return std::pair<T, S>(first, second);
//}

bool isInRoi(sdk::BoundingBox objLocation) {
    bool res = true;
    if (isRoiTest) {
        res = ((objLocation.X1 >= currentRoi.X && objLocation.X2 <= (currentRoi.Width + currentRoi.X))
               && (objLocation.Y1 >= currentRoi.Y && objLocation.Y2 <= (currentRoi.Height + currentRoi.Y)));
        if (!res) {
            std::cout << "Object: [" << objLocation.X1 << ", " << objLocation.Y1 << ", " << objLocation.X2 << ", "
                      << objLocation.Y2 << "]" << std::endl;
            std::cout << "ROI: [" << currentRoi.X << ", " << currentRoi.Y << ", " << currentRoi.Width << ", "
                      << currentRoi.Height << "]" << std::endl;
        }
    }
    return res;
}

void onMessage(const sdk::MessageLog &a_Log, void * /*a_pUserData*/) {
//    std::cout << static_cast<uint32_t>(a_Log.Level) << ": " << a_Log.Text.toString() << std::endl;
}

void onFrameResults(const sdk::FrameResults &a_FrameResults, void * /*a_pUserData*/) { //TODO - find a way to check detections and not tracks
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
                        break;
                    case sdk::FlowSwitcherFlowId::SeaSwir:
                        if (std::find(seaGroups.begin(), seaGroups.end(), trackClass) != seaGroups.end())
                            threshold = currentStream->getFullConfiguration().getSeaSwirDetector().getGroups(
                                    trackClass).getScoreThreshold();
                        break;
                    case sdk::FlowSwitcherFlowId::GroundMwir:
                        if (std::find(groundGroups.begin(), groundGroups.end(), trackClass) != groundGroups.end())
                            threshold = currentStream->getFullConfiguration().getGroundMwirDetector().getGroups(
                                    trackClass).getScoreThreshold();
                        break;
                    case sdk::FlowSwitcherFlowId::GroundRgbAndSwir:
                        if (std::find(groundGroups.begin(), groundGroups.end(), trackClass) != groundGroups.end())
                            threshold = currentStream->getFullConfiguration().getGroundRgbSwirDetector().getGroups(
                                    trackClass).getScoreThreshold();
                        break;
                    default:
                        break;
                }
                BOOST_TEST((track.DetectionData.Score >= threshold || track.DetectionData.Score == 0),
                           "Threshold for class " << trackClass << " [" << threshold << "] > Score ["
                                                  << track.DetectionData.Score <<
                                                  "] --> [" << a_FrameResults.StreamId << ", "
                                                  << currentStream->getId().toString() << "]");
                BOOST_TEST(isInRoi(track.DetectionData.Location));
//        std::string *res = std::find(currentDetectedGroups.begin(), currentDetectedGroups.end(),
//                                                                            track.DetectionData.Class.toString());
//        BOOST_TEST(res != currentDetectedGroups.end(), "Class not in postprocessor");

//        std::cout << "   Expected lowest score: " << score << std::endl;
//        std::cout << "   Track id: " << track.TrackId << std::endl;
//        std::cout << "   Class id: " << track.DetectionData.Class.toString() << std::endl;
//        std::cout << "   Score: " << track.DetectionData.Score << std::endl;
//        std::cout << "   Location: " << (int32_t)track.DetectionData.Location.X1 << "x" << (int32_t)track.DetectionData.Location.Y1 << "-"
//                  << (int32_t)track.DetectionData.Location.X2 << "x" << (int32_t)track.DetectionData.Location.Y2 << std::endl;
//        std::cout << "   Velocity: " << track.Velocity.X << "x" << track.Velocity.Y << std::endl;
//        std::cout << std::endl;
            }
        }
    }
    catch (sdk::Exception &e) {
        BOOST_TEST(((std::string) e.what()).find("Deadline Exceeded") != std::string::npos,
                   "Caught unexpected error: " + (std::string) e.what());
    }

}


void streamErrorTest(const std::string &errorCaught, const std::string &streamId) {
    // If this is not "not registered" and "no data for..."
    if (errorCaught.find("not registered") == std::string::npos
        && errorCaught.find("No data read for 20 seconds, closing stream") == std::string::npos) {
        switch (currentStreamError) {
            case streamError::RotateAngleUnsupported:
                BOOST_TEST_MESSAGE("Rotate angle unsupported test --> " + streamId);
                BOOST_TEST(errorCaught.find("Unsupported rotate angle") != std::string::npos);
                errorCounter = errorCounter > 0 ? errorCounter - 1 : 0;
                break;
            case streamError::RoiIncorrectSize:
                BOOST_TEST_MESSAGE("ROI incorrect size test --> " + streamId);
                BOOST_TEST(errorCaught.find("Unsupported roi") != std::string::npos); //TODO
                errorCounter = errorCounter > 0 ? errorCounter - 1 : 0;
                break;
            case streamError::PreviewError:
                BOOST_TEST_MESSAGE("Preview error test --> " + streamId);
                BOOST_TEST(errorCaught.find("Failed to open display; can't use CVPreview") != std::string::npos);
                errorCounter = errorCounter > 0 ? errorCounter - 1 : 0;
                break;
            default:
                break;
        }
    }
    currentStreamError = streamError::NONE;
}

void outOfRangeErrorTest(const std::string &errorMsg, int minVal, int maxVal) {
    std::string rangeStr;
    rangeStr = "is not in range of " + std::to_string(minVal) + " and " + std::to_string(maxVal);
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

//    std::cout << "running streams:" << std::endl;
//    for (const std::pair stream : runningStreams) {
//        std::cout << stream.first << ", " << stream.second << std::endl;
//    }

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
    ROIUnsupported = "Unsupported roi";
    previewError = "Failed to open display; can't use CVPreview";
    currentRangeError = rangeError::None;
    a_strVideoPath = seaMwirVideo;
    // a_strVideoPath = "rtsp://root:password@172.12.10.199/axis-media/media.amp";
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

//    {
//        std::unique_lock<std::mutex> lock(data.Mutex);
//        data.Condition.wait_for(lock, std::chrono::seconds(10), []() { return data.ServerState; });
//        if (!data.ServerState)
//        {
//            std::cerr << "Couldn't connect to the server for 10 seconds, exiting" << std::endl;
//            exit(-1);
//        }
//    }
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
                return os << "GroundRgbAndSwir";
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

void waitForStreamStarted(const std::shared_ptr<sdk::Stream> &stream) {
//    try {
//        if (currentStream != nullptr) {
//            currentStream.reset();
//            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//        }
//    }
//    catch(...) {std::cout << "Here" << std::endl;}
    std::cout << "Waiting for stream: " << stream->getId().toString() << std::endl;
    std::unique_lock<std::mutex> lock(data.Mutex);
    data.Condition.wait_for(lock, std::chrono::seconds(10), []() { return data.StreamState; });
    if (!data.StreamState) {
        std::cerr << "Couldn't start stream for 10 seconds, exiting" << std::endl;
//        exit(-1);
        throw std::exception();
    }
    currentStream = stream;
}

sdk::StartStreamConfiguration getStartConfiguration(const std::string &customSettings = "stream_settings.bin") {
    sdk::StartStreamConfiguration startConfiguration = mainPipeline->createStartStreamConfiguration(customSettings);
    startConfiguration.getRawSource();
    startConfiguration.getRawSource().setReadTimeoutInSec(20);
    startConfiguration.getGstSink().setUrl(sinkStr);
    return startConfiguration;
}

template<typename CONF, typename T>
void roiTestFunc(bool isUpdate, sdk::FlowSwitcherFlowId flowId) {

    std::cout << printFlow[flowId] << std::endl;
    BOOST_TEST_MESSAGE("Testing ROI out of range...");
//    currentRangeError = rangeError::ROI;
    errorCounter = 0;
    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();

    int minHeight = startConfiguration.getPreprocessor().getRoi().minHeight();
    size_t maxHeight = startConfiguration.getPreprocessor().getRoi().maxHeight();
    int minWidth = startConfiguration.getPreprocessor().getRoi().minWidth();
    size_t maxWidth = startConfiguration.getPreprocessor().getRoi().maxWidth();
    int minX = startConfiguration.getPreprocessor().getRoi().minX();
    size_t maxX = startConfiguration.getPreprocessor().getRoi().maxX();
    int minY = startConfiguration.getPreprocessor().getRoi().minY();
    size_t maxY = startConfiguration.getPreprocessor().getRoi().maxY();


    try {
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        BOOST_TEST_MESSAGE("Height=" + std::to_string(maxHeight + 1));
        errorCounter++;
        startConfiguration.getPreprocessor().getRoi().setHeight(maxHeight + 1);
    }
    catch(const sdk::Exception& e){
        outOfRangeErrorTest(e.what(), minHeight, maxHeight);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    startConfiguration = getStartConfiguration();
    try{
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        BOOST_TEST_MESSAGE("Height=" + std::to_string(minHeight - 1));
        errorCounter++;
        startConfiguration.getPreprocessor().getRoi().setHeight(minHeight - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest(e.what(), minHeight, maxHeight);

    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    startConfiguration = getStartConfiguration();
    try{
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        BOOST_TEST_MESSAGE("Width=" + std::to_string(maxWidth + 1));
        errorCounter++;
        startConfiguration.getPreprocessor().getRoi().setWidth(maxWidth + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest(e.what(), minWidth, maxWidth);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    startConfiguration = getStartConfiguration();
    try{
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        BOOST_TEST_MESSAGE("Width=" + std::to_string(minWidth - 1));
        errorCounter++;
        startConfiguration.getPreprocessor().getRoi().setWidth(minWidth - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest(e.what(), minWidth, maxWidth);

    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    startConfiguration = getStartConfiguration();
    try{
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        BOOST_TEST_MESSAGE("X=" + std::to_string(maxX + 1));
        errorCounter++;
        startConfiguration.getPreprocessor().getRoi().setX(maxX + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest(e.what(), minX, maxX);

    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    startConfiguration = getStartConfiguration();
    try{
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        BOOST_TEST_MESSAGE("X=" + std::to_string(minX - 1));
        errorCounter++;
        startConfiguration.getPreprocessor().getRoi().setX(minX - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest(e.what(), minX, maxX);

    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    startConfiguration = getStartConfiguration();
    try{
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        BOOST_TEST_MESSAGE("Y=" + std::to_string(maxY + 1));
        errorCounter++;
        startConfiguration.getPreprocessor().getRoi().setY(maxY + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest(e.what(), minY, maxY);

    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    startConfiguration = getStartConfiguration();
    try{
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        BOOST_TEST_MESSAGE("Y=" + std::to_string(minY - 1));
        errorCounter++;
        startConfiguration.getPreprocessor().getRoi().setY(minY - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest(e.what(), minY, maxY);
    }

    currentRangeError = rangeError::None;

    try {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        errorCounter = 0;
        BOOST_TEST_MESSAGE("Valid ROI test");
        /**
         * Test valid ROI for 100 frames
         */
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
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
        isRoiTest = true;
        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        sendFrames(stream, 100, nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        stream.reset();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    catch (sdk::Exception &e) {
        BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos, "Caught unexpected error: " + (std::string) e.what());
    }

    try {
        /**
         * Test valid ROI for 100 frames with height/width set to default
         */
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        conf.getRoi().setX(100);
        conf.getRoi().setY(100);
        currentRoi = {
                100,
                100,
                currentFrameDimensions.Width,
                currentFrameDimensions.Height
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

    /**
     * width out of frame
     */
    try {
        BOOST_TEST_MESSAGE("ROI incorrect size test");
        currentStreamError = streamError::RoiIncorrectSize;
//        stream.reset();
        errorCounter = 0;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        conf.getRoi().setX(100);
        conf.getRoi().setWidth((uint32_t)(100 + currentFrameDimensions.Width));
//        conf.getRoi().setHeight(1);  // uncomment to avoid error at end of test
        errorCounter++;
        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        sendFrames(stream, 10, nullptr);
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
        currentStreamError = streamError::RoiIncorrectSize;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        startConfiguration.getFlowSwitcher().setFlowId(flowId);
        CONF conf = getDetectorConfiguration(startConfiguration, (T) 1.0);
        conf.getRoi().setY(100);
        conf.getRoi().setHeight((uint32_t)(100 + currentFrameDimensions.Height));
//        conf.getRoi().setWidth(1); // uncomment to avoid error at end of test
        errorCounter++;
        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
        waitForStreamStarted(stream);
        sendFrames(stream, 10, nullptr);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    catch (sdk::Exception &e) {
        BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos,
                   "Caught unexpected error: " + (std::string) e.what());
    }
    // TODO(NOTE) - We recive here 2 errors because of width/height out of frame but height/width (in accordance) are 0, ask Oleg about this
    BOOST_TEST(errorCounter == 0, "Not all expected errors returned! Number of missed errors: " + std::to_string(errorCounter));
}

template<typename CONF, typename T>
void updateRoiTestFunc(sdk::FlowSwitcherFlowId flowId) {
    std::cout << printFlow[flowId] << std::endl;
    errorCounter = 0;

    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
    startConfiguration.getFlowSwitcher().setFlowId(flowId);
    std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
    waitForStreamStarted(stream);
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    CONF conf = getDetectorUpdateConfiguration(stream, (T) 1.0);

    int minHeight = startConfiguration.getPreprocessor().getRoi().minHeight();
    size_t maxHeight = startConfiguration.getPreprocessor().getRoi().maxHeight();
    int minWidth = startConfiguration.getPreprocessor().getRoi().minWidth();
    size_t maxWidth = startConfiguration.getPreprocessor().getRoi().maxWidth();
    int minX = startConfiguration.getPreprocessor().getRoi().minX();
    size_t maxX = startConfiguration.getPreprocessor().getRoi().maxX();
    int minY = startConfiguration.getPreprocessor().getRoi().minY();
    size_t maxY = startConfiguration.getPreprocessor().getRoi().maxY();

    //checking default values are set properly - can delete it afrerwards
    BOOST_TEST(conf.getRoi().getX() == 0);
    BOOST_TEST(conf.getRoi().getY() == 0);
    BOOST_TEST(conf.getRoi().getWidth() == 0);
    BOOST_TEST(conf.getRoi().getHeight() == 0);

    BOOST_TEST_MESSAGE("Valid Roi update test");
    // TODO - implement valid ROI update
    try{
        BOOST_TEST_MESSAGE("(X:100, Y:100, W:100, H:100)");
        conf.getRoi().setX(100);
        conf.getRoi().setY(100);
        conf.getRoi().setWidth(100);
        conf.getRoi().setHeight(100);
        stream->update();
        BOOST_TEST(conf.getRoi().getX() == 100);
        BOOST_TEST(conf.getRoi().getY() == 100);
        BOOST_TEST(conf.getRoi().getWidth() == 100);
        BOOST_TEST(conf.getRoi().getHeight() == 100);
    }
    catch (const sdk::Exception& e) {
        std::cerr << "Caught unexpected error: " + (std::string) e.what() << std::endl;
    }

    BOOST_TEST_MESSAGE("invalid ROI update test");
    BOOST_TEST_MESSAGE("Out of range ROI update");
    //TODO - ASK Tom why at "first round" I get "out of range" err and second time forward I get "not registered" err ? is valid?
    try{
        BOOST_TEST_MESSAGE("X=" + std::to_string(minX - 1));
        errorCounter++;
//        startConfiguration = getStartConfiguration();
//        stream = mainPipeline->startStream(startConfiguration);
//        std::cout << "The stream is: " + stream->getId().toString() << std::endl;
//        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//        CONF conf = getDetectorUpdateConfiguration(stream, (T) 1.0);
        conf.getRoi().setX(minX - 1);
    }
    catch (const sdk::Exception& e) {
//        std::cout << (std::string) e.what() << std::endl;
        outOfRangeErrorTest((std::string) e.what(), minX, maxX);
//        stream.reset();
    }

    try{
        BOOST_TEST_MESSAGE("X=" + std::to_string(maxX + 1));
        errorCounter++;
//        startConfiguration = getStartConfiguration();
//        startConfiguration.getFlowSwitcher().setFlowId(flowId);
//        stream = mainPipeline->startStream(startConfiguration);
//        std::cout << "The stream is: " + stream->getId().toString() << std::endl;
//        waitForStreamStarted(stream);
//        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//        CONF conf = getDetectorUpdateConfiguration(stream, (T) 1.0);
        conf.getRoi().setX(maxX + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest((std::string) e.what(), minX, maxX);
    }

    try{
        BOOST_TEST_MESSAGE("Y=" + std::to_string(minY -1));
        errorCounter++;
        conf.getRoi().setY(minY - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest(e.what(), minY, maxY);
    }

    try{
        BOOST_TEST_MESSAGE("Y=" + std::to_string(maxY + 1));
        errorCounter++;
        conf.getRoi().setY(maxY + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest(e.what(), minY, maxY);
    }

    try{
        BOOST_TEST_MESSAGE("Width=" + std::to_string(minWidth - 1));
        errorCounter++;
        conf.getRoi().setWidth(minWidth - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest(e.what(), minWidth, maxWidth);
    }

    try{
        BOOST_TEST_MESSAGE("Width=" + std::to_string(maxWidth + 1));
        errorCounter++;
        conf.getRoi().setWidth(maxWidth + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest(e.what(), minWidth, maxWidth);
    }

    try{
        BOOST_TEST_MESSAGE("Height=" + std::to_string(minHeight - 1));
        errorCounter++;
        conf.getRoi().setHeight(minHeight - 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest(e.what(), minHeight, maxHeight);
    }

    try{
        BOOST_TEST_MESSAGE("Height=" + std::to_string(maxHeight + 1));
        errorCounter++;
        conf.getRoi().setHeight(maxHeight + 1);
    }
    catch (const sdk::Exception& e) {
        outOfRangeErrorTest(e.what(), minHeight, maxHeight);
    }

//    BOOST_TEST_MESSAGE("Unsupported ROI update");
    BOOST_TEST(errorCounter == 0, "Not all expected errors returned! Number of missed errors: " + std::to_string(errorCounter));
}

template<typename CONF, typename T>
void thresholdTestFunc(const std::array<std::string, 4> &groups, sdk::FlowSwitcherFlowId flowId) {
    BOOST_TEST_MESSAGE("Valid threshold test");
    /**
     * Threshold
     */
//    if (std::type_index(typeid(T)) == std::type_index(typeid(int))) {
//         groups = seaGroups;
//    }
//    else {
//        groups = groundGroups;
//    }

    for (const std::string &group: groups) {
        try {

            if (group == "None")
                break;
            BOOST_TEST_MESSAGE("Testing group: " + group);
            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
            startConfiguration.getFlowSwitcher().setFlowId(flowId);
            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
            waitForStreamStarted(stream);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            CONF conf = getDetectorUpdateConfiguration(stream, (T) 1.0);
//            conf.getGroups(group).setScoreThreshold(0.89);
//            stream->update();

            sendFrames(stream, 500,
                       [&stream, &conf, &group](uint32_t nFrameId) {
                           float threshold = (float) ((random() % 91) + 10) / 100;
                           if (nFrameId == 50) {
                               BOOST_TEST_MESSAGE(threshold);
                               conf.getGroups(group).setScoreThreshold(threshold);
                               stream->update();
                               currentStream = stream;
                               std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                           } else if (nFrameId == 100) {
                               threshold = (float) ((random() % 91) + 10) / 100;
                               BOOST_TEST_MESSAGE(threshold);
                               conf.getGroups(group).setScoreThreshold(threshold);
                               stream->update();
                               currentStream = stream;
                               std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                           } else if (nFrameId == 150) {
                               threshold = (float) ((random() % 91) + 10) / 100;
                               BOOST_TEST_MESSAGE(threshold);
                               conf.getGroups(group).setScoreThreshold(threshold);
                               stream->update();
                               currentStream = stream;
                               std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                           } else if (nFrameId == 200) {
                               threshold = (float) ((random() % 91) + 10) / 100;
                               BOOST_TEST_MESSAGE(threshold);
                               conf.getGroups(group).setScoreThreshold(threshold);
                               stream->update();
                               currentStream = stream;
                               std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                           } else if (nFrameId == 250) {
                               threshold = (float) ((random() % 91) + 10) / 100;
                               BOOST_TEST_MESSAGE(threshold);
                               conf.getGroups(group).setScoreThreshold(threshold);
                               stream->update();
                               currentStream = stream;
                               std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                           } else if (nFrameId == 300) {
                               threshold = (float) ((random() % 91) + 10) / 100;
                               BOOST_TEST_MESSAGE(threshold);
                               conf.getGroups(group).setScoreThreshold(threshold);
                               stream->update();
                               currentStream = stream;
                               std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                           } else if (nFrameId == 400) {
                               threshold = (float) ((random() % 91) + 10) / 100;
                               BOOST_TEST_MESSAGE(threshold);
                               conf.getGroups(group).setScoreThreshold(threshold);
                               stream->update();
                               currentStream = stream;
                               std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                           }
                       }
            );
            stream.reset();
        }
        catch (sdk::Exception &e) {
            BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos,
                       "Caught unexpected error: " + (std::string) e.what());
        }
    }
}

template<typename CONF, typename T>
void postprocessorTestFunc(const std::array<std::string, 4> &groups) {
    BOOST_TEST_MESSAGE("Postprocessor test");
    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
//    CONF conf = getPostprocessorConfiguration(startConfiguration, (T) 1.0);

}

BOOST_AUTO_TEST_CASE(run_tests) { // NOLINT
        run();
}

BOOST_AUTO_TEST_SUITE(icd_tests) // NOLINT
BOOST_AUTO_TEST_CASE(start_defaults) { // NOLINT
        // boost::unit_test::unit_test_log.set_threshold_level(boost::unit_test::log_messages);
        //    run();
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        /*
         * Raw Source
         */
        //    BOOST_TEST_MESSAGE("Starting test - Raw Source");
        //    BOOST_TEST(startConfiguration.getRawSource().getMode() == sdk::RawSourceMode::Stream);
        //    BOOST_TEST(startConfiguration.getRawSource().getReadTimeoutInSec() == 5);
        //    BOOST_TEST(startConfiguration.getRawSource().getPort() == 0);
        //
        //    /*
        //     * Gc Source
        //     */
        //    BOOST_TEST_MESSAGE("Starting test - Gc Source");
        //    BOOST_TEST(startConfiguration.getGcSource().getTimeoutInSec() == 10);
        //    BOOST_TEST(startConfiguration.getGcSource().getUrl() == "");
        //    BOOST_TEST(startConfiguration.getGcSource().getDeviceAccessMode() == sdk::DeviceAccess::Control);
        //    BOOST_TEST(startConfiguration.getGcSource().sizeGcParameters() == 0);
        //
        //    /*
        //     * Gst Source
        //     */
        //    BOOST_TEST_MESSAGE("Starting test - Gst Source");
        //    BOOST_TEST(startConfiguration.getGstSource().getSource().toString() == "rtspsrc location={}");
        //    BOOST_TEST(startConfiguration.getGstSource().getUrl() == "");

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
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("light-vehicle").getScoreThreshold() == 0.3f);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("light-vehicle").getMinWidth() == 1);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("light-vehicle").getMaxWidth() == 1000);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("light-vehicle").getMinHeight() == 1);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("light-vehicle").getMaxHeight() == 1000);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("light-vehicle").getMinAspectRatio() == 0);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("light-vehicle").getMaxAspectRatio() == 100);
        // person
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("person").getScoreThreshold() == 0.3f);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("person").getMinWidth() == 1);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("person").getMaxWidth() == 1000);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("person").getMinHeight() == 1);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("person").getMaxHeight() == 1000);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("person").getMinAspectRatio() == 0);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("person").getMaxAspectRatio() == 100);
        // two-wheeled
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("two-wheeled").getScoreThreshold() == 0.3f);
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
         * Ground RgbAndSwir Flow ID
         */
        startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
        BOOST_TEST_MESSAGE("Starting test - Ground RgbAndSwir Flow ID");
        BOOST_TEST(startConfiguration.getFlowSwitcher().getFlowId() == sdk::FlowSwitcherFlowId::GroundRgbAndSwir);

        /*
         * Ground RgbAndSwir Detector
         */
        BOOST_TEST_MESSAGE("Starting test - Ground RgbAndSwir Detector");
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getRoi().getWidth() == 0);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getRoi().getHeight() == 0);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getRoi().getX() == 0);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getRoi().getY() == 0);
        // light-vehicle
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("light-vehicle").getScoreThreshold() == 0.4f);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("light-vehicle").getMinWidth() == 1);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("light-vehicle").getMaxWidth() == 1000);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("light-vehicle").getMinHeight() == 1);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("light-vehicle").getMaxHeight() == 1000);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("light-vehicle").getMinAspectRatio() == 0);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("light-vehicle").getMaxAspectRatio() == 100);
        // person
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("person").getScoreThreshold() == 0.6f);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("person").getMinWidth() == 1);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("person").getMaxWidth() == 1000);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("person").getMinHeight() == 1);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("person").getMaxHeight() == 1000);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("person").getMinAspectRatio() == 0);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("person").getMaxAspectRatio() == 100);
        // two-wheeled
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("two-wheeled").getScoreThreshold() == 0.4f);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("two-wheeled").getMinWidth() == 1);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("two-wheeled").getMaxWidth() == 1000);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("two-wheeled").getMinHeight() == 1);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("two-wheeled").getMaxHeight() == 1000);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("two-wheeled").getMinAspectRatio() == 0);
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getGroups("two-wheeled").getMaxAspectRatio() == 100);

        /*
         * Ground Rgb Postprocessor
         */
        BOOST_TEST_MESSAGE("Starting test - Ground RgbAndSwir Postprocessor");
        BOOST_TEST(startConfiguration.getGroundRgbSwirDetector().getOutputClasses(0) == "*");

        /*
         * Ground Rgb Tracker
         */
        BOOST_TEST_MESSAGE("Starting test - Ground RgbAndSwir Tracker");
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
         * Tracks Publisher
         */
        BOOST_TEST_MESSAGE("Starting test - Tracks Publisher");
        BOOST_TEST(startConfiguration.getTracksPublisher().getSourceData() == sdk::PublisherDataType::Tracks);
        //BOOST_TEST(startConfiguration.getTracksPublisher().getTopicName() == "FrameResults");
        //BOOST_TEST(startConfiguration.getTracksPublisher().getDomainId() == 200);

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
        BOOST_TEST(startConfiguration.getRenderer().getBoundingBoxes("").getTextOffsetY() == -5);
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
        BOOST_TEST(startConfiguration.getDebugModule().getEnable() == false);

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

        BOOST_TEST_MESSAGE("Starting test - RangeEstimator");
        BOOST_TEST(startConfiguration.getRangeEstimator().getWorldObjectSize(0).getClassName() == "person");
        BOOST_TEST(startConfiguration.getRangeEstimator().getWorldObjectSize(0).getWidthInMeters() == 0);
        BOOST_TEST(startConfiguration.getRangeEstimator().getWorldObjectSize(0).getHeightInMeters() == 1.8f);

}
BOOST_AUTO_TEST_SUITE_END() // NOLINT icd_tests


BOOST_AUTO_TEST_SUITE(icd_tests1) // NOLINT
BOOST_AUTO_TEST_SUITE(rotate_angle) // NOLINT
BOOST_AUTO_TEST_CASE(valid_rotate_angle) { // NOLINT
        try {

            // boost::unit_test::unit_test_log.set_threshold_level(boost::unit_test::log_messages);
            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
            BOOST_TEST_MESSAGE("Valid rotate angle test");
            BOOST_TEST_MESSAGE("0");
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
                    sendFrames(stream, 15,
                               [&stream, angle](uint32_t nFrameId) {
                                   if (nFrameId == 5)
                                       BOOST_TEST(stream->getFullConfiguration().getPreprocessor().getRotateAngle() ==
                                                  angle);
                               }
                    );
//                std::this_thread::sleep_for(std::chrono::milliseconds(1000));

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
}

BOOST_AUTO_TEST_SUITE(invalid_rotate_angle) // NOLINT
    BOOST_AUTO_TEST_CASE(rotate_angle_out_of_range) {
        BOOST_TEST_MESSAGE("Invalid rotate angle - Out of range test");
        currentRangeError = rangeError::RotateAngle;
        errorCounter++;
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
        try {
            BOOST_TEST_MESSAGE(startConfiguration.getPreprocessor().minRotateAngle() - 1);
            startConfiguration.getPreprocessor().setRotateAngle(startConfiguration.getPreprocessor().minRotateAngle() - 1);
        }
        catch (sdk::Exception& e) {
            outOfRangeErrorTest(e.what(), startConfiguration.getPreprocessor().minRotateAngle(), startConfiguration.getPreprocessor().maxRotateAngle());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        errorCounter++;
        startConfiguration = getStartConfiguration();
        try {
            BOOST_TEST_MESSAGE(startConfiguration.getPreprocessor().maxRotateAngle() + 1);
            startConfiguration.getPreprocessor().setRotateAngle(startConfiguration.getPreprocessor().maxRotateAngle() + 1);
        }
        catch (sdk::Exception& e) {
            outOfRangeErrorTest(e.what(), startConfiguration.getPreprocessor().minRotateAngle(), startConfiguration.getPreprocessor().maxRotateAngle());
        }
        BOOST_TEST(errorCounter == 0, "Not all expected errors returned! Number of missed errors: " + std::to_string(errorCounter));
        currentRangeError = rangeError::None;
    }
    BOOST_AUTO_TEST_CASE(rotate_angle_unsupported) { // NOLINT
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();

        BOOST_TEST_MESSAGE("Invalid rotate angle - Unsupported test");
        int angle;
        for (int i = 0; i < 9; i++) {
            try {
                currentStreamError = streamError::RotateAngleUnsupported;
                std::string currentErrorStr = rotateAngleUnsupported;
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
BOOST_AUTO_TEST_SUITE_END() // NOLINT invalid_rotate_angle
BOOST_AUTO_TEST_SUITE_END() // NOLINT rotate_angle

BOOST_AUTO_TEST_CASE(preprocessor_roi){ // no unsupported?
    sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
    roiTestFunc<typeof(startConfiguration.getPreprocessor()), size_t>(false, sdk::FlowSwitcherFlowId::Unspecified);
}

// TODO - write the threshold test func (same logic as ROI test)
//BOOST_AUTO_TEST_SUITE(threshold)
//
//BOOST_AUTO_TEST_CASE(valid_threshold){
//
//}
//BOOST_AUTO_TEST_SUITE_END() // NOLINT threshold


//        BOOST_AUTO_TEST_CASE(rotate_angle_update) { // NOLINT
//            errorCounter = 0;
//            currentStreamError = streamError::NONE;
//            // boost::unit_test::unit_test_log.set_threshold_level(boost::unit_test::log_messages);
//            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
//            BOOST_TEST_MESSAGE("Rotate angle update test");
//            BOOST_TEST_MESSAGE("Valid angle");
//            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
//            waitForStreamStarted(stream);
//            BOOST_TEST(stream->getConfiguration().getPreprocessor().getRotateAngle() == 0);
//            int angle;
//            for (int i = 1; i < 4; i++) {
//                angle = i * 90;
//                stream->getConfiguration().getPreprocessor().setRotateAngle(angle);
//                stream->update();
//                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//                BOOST_TEST(stream->getConfiguration().getPreprocessor().getRotateAngle() == angle);
//            }
//            stream.reset();
//            BOOST_TEST_MESSAGE("Invalid angle");
//
//            for (int i = 0; i < 10; i++) {
//                currentStreamError = streamError::RotateAngleNotInRange;
//                angle = (int) (random() % 360) + 1;
//                BOOST_TEST_MESSAGE(angle);
//                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
//                startConfiguration = getStartConfiguration();
//                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//                errorCounter++;
//                stream = mainPipeline->startStream(startConfiguration);
//                if (angle <= 270) {
//                    currentStreamError = streamError::RotateAngleUnsupported;
//                }
//                else if (angle == 90 || angle == 180 || angle == 270) {
//                    errorCounter = errorCounter > 0 ? errorCounter - 1 : 0;
//                    continue;
//                }
//                waitForStreamStarted(stream);
//                sendFrames(stream, 30,
//                    [&angle, &stream](uint32_t nFrameId) {
//                        if (nFrameId == 5) {
//                            stream->getConfiguration().getPreprocessor().setRotateAngle(angle);
//                            stream->update();
//                        }
//                    }
//                );
//
//            }
//            stream.reset();
//            currentStreamError = streamError::NONE;
//            BOOST_TEST(errorCounter == 0, "Not all expected errors returned! Number of missed errors: " + std::to_string(errorCounter));
//        }

//BOOST_AUTO_TEST_CASE(preprocessor_roi) { // NOLINT
//        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
//        roiTestFunc<typeof(startConfiguration.getPreprocessor()), size_t>(false, sdk::FlowSwitcherFlowId::SeaMwir);
//}

BOOST_AUTO_TEST_CASE(flow_switcher) { // NOLINT
        try {

            BOOST_TEST_MESSAGE("Flow switcher test");
            /**
             * Update test
             */
            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
            waitForStreamStarted(stream);
            sendFrames(stream, 50,
                       [&stream](uint32_t nFrameId) {
                           if (nFrameId == 10) {
                               BOOST_TEST_MESSAGE(stream->getConfiguration().getFlowSwitcher().getFlowId());
                               stream->getConfiguration().getFlowSwitcher().setFlowId(
                                       sdk::FlowSwitcherFlowId::GroundMwir);
                               stream->update();
                               BOOST_TEST_MESSAGE(stream->getConfiguration().getFlowSwitcher().getFlowId());
                               BOOST_TEST(stream->getConfiguration().getFlowSwitcher().getFlowId() ==
                                          sdk::FlowSwitcherFlowId::GroundMwir);

                           } else if (nFrameId == 20) {
                               BOOST_TEST_MESSAGE(stream->getConfiguration().getFlowSwitcher().getFlowId());
                               stream->getConfiguration().getFlowSwitcher().setFlowId(
                                       sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
                               stream->update();
                               BOOST_TEST_MESSAGE(stream->getConfiguration().getFlowSwitcher().getFlowId());
                               BOOST_TEST(stream->getConfiguration().getFlowSwitcher().getFlowId() ==
                                          sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
                           } else if (nFrameId == 30) {
                               BOOST_TEST_MESSAGE(stream->getConfiguration().getFlowSwitcher().getFlowId());
                               stream->getConfiguration().getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::SeaSwir);
                               stream->update();
                               BOOST_TEST_MESSAGE(stream->getConfiguration().getFlowSwitcher().getFlowId());
                               BOOST_TEST(stream->getConfiguration().getFlowSwitcher().getFlowId() ==
                                          sdk::FlowSwitcherFlowId::SeaSwir);
                           } else if (nFrameId == 40) {
                               BOOST_TEST_MESSAGE(stream->getConfiguration().getFlowSwitcher().getFlowId());
                               stream->getConfiguration().getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::SeaMwir);
                               stream->update();
                               BOOST_TEST_MESSAGE(stream->getConfiguration().getFlowSwitcher().getFlowId());
                               BOOST_TEST(stream->getConfiguration().getFlowSwitcher().getFlowId() ==
                                          sdk::FlowSwitcherFlowId::SeaMwir);
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
BOOST_AUTO_TEST_SUITE(detector_configuration) // NOLINT

BOOST_AUTO_TEST_CASE(detector_roi_start) {  // NOLINT
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
//            BOOST_TEST_MESSAGE("Sea Mwir detector ROI test");
        roiTestFunc<typeof(startConfiguration.getSeaMwirDetector()), int>(true, sdk::FlowSwitcherFlowId::SeaMwir);
//            BOOST_TEST_MESSAGE("Ground MWIR detector ROI test");
        a_strVideoPath = groundMwirVideo;
        startConfiguration = getStartConfiguration();
        roiTestFunc<typeof(startConfiguration.getGroundMwirDetector()), float>(true, sdk::FlowSwitcherFlowId::GroundMwir);
//            BOOST_TEST_MESSAGE("Ground RGB detector ROI test");
        a_strVideoPath = groundRgbVideo;
        startConfiguration = getStartConfiguration();
        roiTestFunc<typeof(startConfiguration.getGroundRgbSwirDetector()), double>(true, sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
        a_strVideoPath = seaSwirVideo;
        startConfiguration = getStartConfiguration();
        roiTestFunc<typeof(startConfiguration.getSeaSwirDetector()), uint32_t>(true, sdk::FlowSwitcherFlowId::SeaSwir);
}

BOOST_AUTO_TEST_CASE(detector_roi_update) {  // NOLINT
        a_strVideoPath = seaMwirVideo;
        updateRoiTestFunc<typeof(sdk::SeaMwirDetectorUpdateStreamConfiguration), int>(sdk::FlowSwitcherFlowId::SeaMwir);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        a_strVideoPath = groundMwirVideo;
        updateRoiTestFunc<typeof(sdk::GroundMwirDetectorUpdateStreamConfiguration), float>(sdk::FlowSwitcherFlowId::GroundMwir);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        a_strVideoPath = groundSwirVideo;
        updateRoiTestFunc<typeof(sdk::GroundRgbSwirDetectorUpdateStreamConfiguration), double>(sdk::FlowSwitcherFlowId::GroundRgbAndSwir);

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        a_strVideoPath = seaSwirVideo;
        updateRoiTestFunc<typeof(sdk::SeaSwirDetectorUpdateStreamConfiguration), uint32_t>(sdk::FlowSwitcherFlowId::SeaSwir);
}

BOOST_AUTO_TEST_CASE(detector_groups) { // NOLINT
        BOOST_TEST_MESSAGE("Starting detector groups test");
        try {
            a_strVideoPath = seaMwirVideo;
            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
            thresholdTestFunc<typeof(stream->getConfiguration().getSeaMwirDetector()), int>(
                    seaGroups, sdk::FlowSwitcherFlowId::SeaMwir);
        }
        catch (sdk::Exception& e) {
            BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos,
                       "Caught unexpected error: " + (std::string) e.what());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        try {
            a_strVideoPath = groundMwirVideo;
            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
//                startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundMwir);
            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
            thresholdTestFunc<typeof(stream->getConfiguration().getGroundMwirDetector()), float>(
                    groundGroups, sdk::FlowSwitcherFlowId::GroundMwir);
        }
        catch (sdk::Exception& e) {
            BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos,
                       "Caught unexpected error: " + (std::string) e.what());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        try {
            a_strVideoPath = groundRgbVideo;
            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
//                startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
            thresholdTestFunc<typeof(stream->getConfiguration().getGroundRgbSwirDetector()), double>(
                    groundGroups, sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
        }
        catch (sdk::Exception& e) {
            BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos,
                       "Caught unexpected error: " + (std::string) e.what());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        try {
            a_strVideoPath = seaSwirVideo;
            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
//                startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::SeaSwir);
            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
            thresholdTestFunc<typeof(stream->getConfiguration().getSeaSwirDetector()), uint32_t>(
                    seaGroups, sdk::FlowSwitcherFlowId::SeaSwir);
        }
        catch (sdk::Exception& e) {
            BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos,
                       "Caught unexpected error: " + (std::string) e.what());
        }
}

BOOST_AUTO_TEST_SUITE_END() // NOLINT

//    BOOST_AUTO_TEST_SUITE(postprocessor) // NOLINT
//        BOOST_AUTO_TEST_CASE(postprocessor_configuration) {  // NOLINT
//            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
//
//            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
//
//        }
//    BOOST_AUTO_TEST_SUITE_END() // NOLINT

BOOST_AUTO_TEST_SUITE_END() // NOLINT


BOOST_AUTO_TEST_SUITE(renderer) // NOLINT

BOOST_AUTO_TEST_CASE(renderer_general) { // NOLINT
        try {

            //        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
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
            BOOST_TEST(((std::string) e.what()).find("not registered") != std::string::npos,
                       "Caught unexpected error: " + (std::string) e.what());
        }
}

BOOST_AUTO_TEST_SUITE_END() // NOLINT

BOOST_AUTO_TEST_SUITE(output) //NOLINT
    BOOST_AUTO_TEST_CASE(full_video) { //NOLINT
        BOOST_TEST_MESSAGE("Running full videos");
        try {

            BOOST_TEST_MESSAGE("Sea MWIR");
            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
            a_strVideoPath = seaMwirVideo;
            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
            waitForStreamStarted(stream);
            sendFrames(stream, -1, nullptr);

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            BOOST_TEST_MESSAGE("Ground MWIR");
            startConfiguration = getStartConfiguration();
            startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundMwir);
            a_strVideoPath = groundMwirVideo;
            stream = mainPipeline->startStream(startConfiguration);
            waitForStreamStarted(stream);
            sendFrames(stream, -1, nullptr);

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            BOOST_TEST_MESSAGE("Sea SWIR");
            startConfiguration = getStartConfiguration();
            startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::SeaSwir);
            a_strVideoPath = seaSwirVideo;
            stream = mainPipeline->startStream(startConfiguration);
            waitForStreamStarted(stream);
            sendFrames(stream, -1, nullptr);

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            BOOST_TEST_MESSAGE("Ground RGB");
            startConfiguration = getStartConfiguration();
            startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
            a_strVideoPath = groundRgbVideo;
            stream = mainPipeline->startStream(startConfiguration);
            waitForStreamStarted(stream);
            sendFrames(stream, -1, nullptr);

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            BOOST_TEST_MESSAGE("Ground SWIR");
            startConfiguration = getStartConfiguration();
            a_strVideoPath = groundSwirVideo;
            stream = mainPipeline->startStream(startConfiguration);
            waitForStreamStarted(stream);
            sendFrames(stream, -1, nullptr);

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            stream.reset();
        }
        catch (sdk::Exception& e) {
            BOOST_TEST(((std::string) e.what()).find("Deadline Exceeded") != std::string::npos,
                       "Caught unexpected error: " + (std::string) e.what());
        }
    }

    BOOST_AUTO_TEST_CASE(output_preview) { //NOLINT
        try {

            sdk::StartStreamConfiguration startConfiguration = getStartConfiguration();
            startConfiguration.getPreviewModule().setEnable(true);
            currentStreamError = streamError::PreviewError;
            errorCounter++;
            std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);
            waitForStreamStarted(stream);
            sendFrames(stream, 10, nullptr);
            BOOST_TEST(errorCounter == 0, "Not all expected errors returned! Number of missed errors: " + std::to_string(errorCounter));
        }
        catch (const std::exception &e) {
            std::cout << e.what() << std::endl;
        }
    }

BOOST_AUTO_TEST_SUITE_END() // NOLINT

BOOST_AUTO_TEST_SUITE(debug) // NOLINT
    BOOST_AUTO_TEST_CASE(debug_environment) { // NOLINT
        std::string settingsFile = "/home/tom/Desktop/sdk_runner/stream_settings.bin";
        sdk::StartStreamConfiguration startConfiguration = getStartConfiguration(settingsFile);
        a_strVideoPath = groundMwirVideo;
        BOOST_TEST(startConfiguration.getFlowSwitcher().getFlowId() == sdk::FlowSwitcherFlowId::GroundMwir);
        BOOST_TEST(startConfiguration.getGroundMwirDetector().getGroups("light-vehicle").getScoreThreshold() == 0.1f);
        BOOST_TEST(startConfiguration.getRenderer().getOsd().getSkipRendering() == true);

        std::shared_ptr<sdk::Stream> stream = mainPipeline->startStream(startConfiguration);

        waitForStreamStarted(stream);
        BOOST_TEST_MESSAGE("Check that SightX logo is in the bottom left corner");
        sendFrames(stream, 1000, nullptr);
    }

BOOST_AUTO_TEST_SUITE_END() // NOLINT

BOOST_AUTO_TEST_CASE(destroy) { // NOLINT
        mainPipeline.reset();
}

BOOST_AUTO_TEST_CASE(test){

}
