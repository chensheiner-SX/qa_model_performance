#include "PipelineInterface.h"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <thread>

#include <opencv2/videoio.hpp>

#include <string>
#include <fstream>
#include <dirent.h>
#include <vector>
#include <algorithm>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>
#include <signal.h>

using namespace sightx;
using namespace std;

ofstream outfile;

struct UserData
{
    bool ServerState = false;
    bool StreamState = false;

    std::mutex              Mutex;
    std::condition_variable Condition;
};

void onMessage(const sdk::MessageLog& a_Log, void* /*a_pUserData*/)
{
    std::cout << static_cast<uint32_t>(a_Log.Level) << ": " << a_Log.Text.toString() << std::endl;
}

void onFrameResults(const sdk::FrameResults& a_FrameResults, void* /*a_pUserData*/)
{
    std::cout << "Frame " << a_FrameResults.FrameId << " in stream " << a_FrameResults.StreamId.toString() << std::endl;
    for (const sdk::Track& track : a_FrameResults.Tracks.toVector())
    {
        std::cout << "   Track id: " << track.TrackId << std::endl;
        std::cout << "   Class id: " << track.DetectionData.Class.toString() << std::endl;
        std::cout << "   Score: " << track.DetectionData.Score << std::endl;
        std::cout << "   Location: " << (int32_t)track.DetectionData.Location.X1 << "x" << (int32_t)track.DetectionData.Location.Y1 << "-"
                  << (int32_t)track.DetectionData.Location.X2 << "x" << (int32_t)track.DetectionData.Location.Y2 << std::endl;
        std::cout << "   Velocity: " << track.Velocity.X << "x" << track.Velocity.Y << std::endl;
        std::cout << std::endl;
        outfile << to_string(a_FrameResults.FrameId) << ","                 // Frame id
                << track.DetectionData.Class.toString() << ","              // class
                << track.TrackId << ","                                     // track_id
                <<  track.DetectionData.Score << ","                        // score
                << (int32_t)track.DetectionData.Location.X1 << ","         //x1
                << (int32_t)track.DetectionData.Location.Y1 << ","             //y1
                << (int32_t)track.DetectionData.Location.X2 << ","           //x2
                << (int32_t)track.DetectionData.Location.Y2 << ","            //y2
                << track.Velocity.X << ","                           //velocity X
                << track.Velocity.Y                                 //velocity Y
                << endl;


    }

}

void onStreamEvent(const sdk::StreamLog& a_StreamLog, void* a_pUserData)
{
    UserData* pUserData = static_cast<UserData*>(
        a_pUserData);

    std::lock_guard<std::mutex> lock(pUserData->Mutex);

    switch (a_StreamLog.Event)
    {
    case sdk::StreamEvent::Created:
        std::cout << "Stream " << a_StreamLog.StreamId.toString() << " created" << std::endl;
        break;
    case sdk::StreamEvent::Started:
        std::cout << "Stream " << a_StreamLog.StreamId.toString() << " started" << std::endl;
        break;
    case sdk::StreamEvent::Connected:
        std::cout << "Stream " << a_StreamLog.StreamId.toString() << " connected" << std::endl;
        pUserData->StreamState = true;
        break;
    case sdk::StreamEvent::Stopped:
        std::cout << "Stream " << a_StreamLog.StreamId.toString() << " stopped" << std::endl;
        pUserData->StreamState = false;
        break;
    case sdk::StreamEvent::Error:
        std::cout << "Stream " << a_StreamLog.StreamId.toString() << " got error at module " << a_StreamLog.ModuleName.toString() << ": " << a_StreamLog.Text.toString() << std::endl;
        break;
    case sdk::StreamEvent::EOS:
        std::cout << "Stream " << a_StreamLog.StreamId.toString() << " got end-of-stream" << std::endl;
        break;
    case sdk::StreamEvent::Terminated:
        std::cout << "Stream " << a_StreamLog.StreamId.toString() << " terminated" << std::endl;
        break;
    }

    pUserData->Condition.notify_one();
}

void onServerStateChange(sdk::ServerState a_eServerState, void* a_pUserData)
{
    switch (a_eServerState)
    {
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

    UserData* pUserData = static_cast<UserData*>(
        a_pUserData);

    {
        std::lock_guard<std::mutex> lock(pUserData->Mutex);
        pUserData->ServerState = a_eServerState == sdk::ServerState::ServerUp;
    }

    pUserData->Condition.notify_one();
}

void run(
    const char* a_strServerIP,
    const char* a_strVideoPath,
    //const string a_strVideoPath,
    const string flow,
    const string pixel_avg,
    const string pixel_std)
   // )
{
    UserData data;

    // init pipeline
    sdk::GrpcSettings grpcSettings;
    grpcSettings.ServerIP = a_strServerIP;

    sdk::Callbacks callbacks;
    callbacks.MinLogLevel         = sdk::LogLevel::Warning;
    callbacks.OnMessage           = &onMessage;
    callbacks.OnFrameResults      = &onFrameResults;
    callbacks.OnStreamEvent       = &onStreamEvent;
    callbacks.OnServerStateChange = &onServerStateChange;
    callbacks.UserData            = &data;

    std::shared_ptr<sdk::Pipeline> pPipeline = sdk::Pipeline::create(
        grpcSettings,
        {}, // dds settings
        {}, // raw stream settings
        callbacks);

    // wait for server up event
    {
        std::unique_lock<std::mutex> lock(data.Mutex);
        data.Condition.wait_for(lock, std::chrono::seconds(10), [&data]() { return data.ServerState; });
        if (!data.ServerState)
        {
            std::cerr << "Couldn't connect to the server for 10 seconds, exiting" << std::endl;
            exit(-1);
        }
    }

    // start new stream
    sdk::StartStreamConfiguration startConfiguration = pPipeline->createStartStreamConfiguration();
    startConfiguration.getRawSource(); // raw source needs to be 'touched' to enable it, if no internal settings need to be changed
    startConfiguration.getTelemetry(); // telemetry needs to be 'touched' to enable it if it is going to be used
    //startConfiguration.getDebugModule().setEnable(true); // uncomment to enable debug mode; debug data is stored under ~/sightx_debug
//    startConfiguration.getGstSink().setUrl("rtsp://172.12.10.29:8554/test"); // uncomment to stream preview to url; rtsp server should be available at <ip>
//    startConfiguration.getPreviewModule().setEnable(true); // uncomment to enable local preview; docker should be able to access X for this to work
//    startConfiguration.getRenderer().setDrawDetections(true); // uncomment to draw detections in the debug/preview/gstsink
    //sdk::Vector<sdk::String> vGroups = startConfiguration.getDetector().keysGroups(); // uncomment to receive the list of supported classes for this detector
    //startConfiguration.getDetector().getGroups("people").setScoreThreshold(0.15f); // uncomment to change detector threshold (don't set less than 0.1)
//    startConfiguration.getDetector().appendAveragePixelValue(0.4107f); // uncomment to change detector average pixel value
//    startConfiguration.getDetector().appendPixelValueStandardDeviation(0.053f); // uncomment to change detector pixel value standard deviation
    //startConfiguration.getRangeEstimator().getWorldObjectSize(0).setHeightInMeters(1.75f); // uncomment to change the height of person
    //startConfiguration.getTrackerRate().setOutputFramerate(30); // uncomment to change the output framerate
    if(flow == "rgb")
        startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
    else
        startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundMwir);
    if (stod(pixel_avg)!=0)
    {
        startConfiguration.getGroundMwirDetector().appendAveragePixelValue(stod(pixel_avg));
        startConfiguration.getGroundMwirDetector().appendPixelValueStandardDeviation(stod(pixel_std));
        }
//    startConfiguration.getGroundMwirDetector().appendAveragePixelValue(0.425818);
//    startConfiguration.getGroundMwirDetector().appendPixelValueStandardDeviation(0.00712599);

    std::shared_ptr<sdk::Stream> pStream = pPipeline->startStream(
        startConfiguration);

    // update stream
    //pStream->getConfiguration().getRenderer().setDrawDetections(true); // uncomment to toggle draw detections while stream is running
    pStream->update();

    // wait for stream connected event
    {
        std::unique_lock<std::mutex> lock(data.Mutex);
        data.Condition.wait(lock, [&data]() { return data.StreamState; });
    }

    outfile << "frame_id,class,track_id,score,x1,y1,x2,y2,vel_x,vel_y" << endl;
    // push raw video frames
    cv::VideoCapture capture(
        a_strVideoPath);
    // /home/chen/PycharmProjects/qa-scripts/model_performance_data/data/AshdodPort_D20210802_Pn2_SSummer_N00096_CtNone_H035_LcSunlight_Ds02500_LtCalm_LfSea_VrNone_StStaged_LdFrontLight_B02_V00_Js02_P07_T28_MWIR_unknown-8bit/%05d.png
    uint32_t nFrameId = 0;
    cv::Mat  matFrame;
    while (capture.read(matFrame))
    {
        // push frame
        pStream->pushRawFrame(
            sdk::RawSourceId::RawSource, // ids are defined in Configuration.h
            matFrame.data,
            matFrame.cols,
            matFrame.rows,
            sdk::PixelFormat::BGR8, // supported pixel formats are defined in Types.h
            matFrame.step[0],
            nFrameId++, // can be default (-1) to use frame ids automatically generated by the engine
                        // this frame id is used in track callback
            -1);        // timestamp in ms, if -1 current time is used

        // update telemetry
        sdk::TelemetryInfo telemetry;
        telemetry.HorizontalFOVMrads = 10;
        telemetry.VerticalFOVMrads   = 10;

        pStream->updateTelemetry(
            telemetry,
            -1); // timestamp in ms, if -1 current time is used; should match frame timestamps

        std::this_thread::sleep_for(
            std::chrono::milliseconds(30));
    }

    // stop stream (optional, will be destroyed on scope exit)
    pStream.reset();

    // destroy pipeline interface (optional, will be destroyed on scope exit)
    pPipeline.reset();
}
//
void changeToLower(string& data){
    std::for_each(
            data.begin(),
            data.end(),
            [](char & c) {
                c = ::tolower(c);
            });
}


int main(int argc, char** argv)
{
    // discover running servers
    sdk::Vector<sdk::ServerInfo> vServers = sdk::Pipeline::discover();
    std::cout << "Discovered servers:" << std::endl;
    for (const sdk::ServerInfo& info : vServers.toVector())
    {
        std::cout << info.HostName.toString() << " " << info.IP.toString() << ":" << info.Port << std::endl;
    }

    if (argc != 7)
//    if (argc != 3)
    {
        std::cerr << "Syntax: ./sdk_sample server_ip frames_folder_path flow_id output_csv_path mean_normalization std_normalization" << std::endl;
        exit(-1);
    }
//    std::string video_path = static_cast<std::string>(argv[2]);

    std::string flow = static_cast<std::string>(argv[3]);
    changeToLower(flow);

    std::string csv_output_path = static_cast<std::string>(argv[4]);
    std::string pixel_avg = static_cast<std::string>(argv[5]);
    std::string pixel_std = static_cast<std::string>(argv[6]);


    std::cout <<"mean "<< pixel_avg<< std::endl;
    std::cout <<"std "<< pixel_std<< std::endl;

    // Changing ~/ in path to /home/$USER
    size_t pos = csv_output_path.find("~/");
    if(pos != std::string::npos){
        csv_output_path.replace(pos, 2, static_cast<std::string>("/home/") + std::getenv("USER"));
    }

//    pos = video_path.find("~/");
//    if(pos != std::string::npos){
//        video_path.replace(pos, 2, static_cast<std::string>("/home/") + std::getenv("USER"));
//    }

//    csv_output_path += "/meerkat_detections.csv";
    outfile.open(csv_output_path);

    try
    {
        run(argv[1], argv[2], flow,pixel_avg, pixel_std);
//        run(
//            argv[1],
//            argv[2]);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}

