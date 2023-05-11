#include "PipelineInterface.h"

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <thread>
#include <string>
#include <fstream>
#include <dirent.h>
#include <vector>
#include <algorithm>
#include <cstring>
#include <sys/stat.h>
#include <opencv2/opencv.hpp>
#include <unistd.h>
#include <cstdlib>
#include <signal.h>

using namespace sightx;
using namespace std;

//std::string csv_path;

// TODO: add file path as argument,, from where we run the main python script
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
//    std::cout << static_cast<uint32_t>(a_Log.Level) << ": " << a_Log.Text.toString() << std::endl;
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

void signal_callback_handler(int signum) {
    cout << "\n\nCaught signal " << signum << "\nExiting..." << endl;
    // Terminate program
    exit(signum);
}

void run(
    const char* a_strServerIP,
    const string a_strFramesPath,
    const string flow
    )
{
    UserData data;

    // init pipeline
    sdk::GrpcSettings grpcSettings;
    grpcSettings.ServerIP = a_strServerIP;

    sdk::Callbacks callbacks;
    callbacks.MinLogLevel         = sdk::LogLevel::Warning;
    callbacks.OnMessage           = &onMessage;
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
    startConfiguration.getRawSource().setMode(sdk::RawSourceMode::SingleFrame); // set single frame mode.
    startConfiguration.getRawSource().setReadTimeoutInSec(5); // disable read timeout. Not needed when using single frame mode.
    startConfiguration.getTracksPublisher().setSourceData(sdk::PublisherDataType::Detections);
    if(flow == "rgb")
        startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundRgbAndSwir);
    else
        startConfiguration.getFlowSwitcher().setFlowId(sdk::FlowSwitcherFlowId::GroundMwir);


    std::shared_ptr<sdk::Stream> pStream = pPipeline->startStream(
        startConfiguration);


    // wait for stream started event
    {
        std::unique_lock<std::mutex> lock(data.Mutex);
        data.Condition.wait(lock, [&data]() { return data.StreamState; });
    }

    signal(SIGINT, signal_callback_handler); // handler for ctrl+c interrupt

    string path = a_strFramesPath;
    DIR* directory = opendir(path.c_str());

    if (directory == NULL) {
        cout << "Error: Could not open directory " << path << endl;
        exit(1);
    }

    vector<string> files;
    struct dirent* file;
    while ((file = readdir(directory)) != NULL) {
        files.push_back(file->d_name);
    }

    sort(files.begin(), files.end());

    outfile << "framdId,class,x1,y1,x2,y2" << endl;
    int fileCounter = 1;

    for (const string& filename : files) {
        string extension;
        size_t dotPos = filename.rfind(".");
        if (dotPos != string::npos) {
            extension = filename.substr(dotPos + 1);
        }
        if(extension == "" || extension == "json" || extension == "mp4" || extension == "avi")
            continue;
        string filePath = path + "/" + filename;
        char resolvedPath[PATH_MAX];
        if (realpath(filePath.c_str(), resolvedPath) == NULL) {
            cout << "Error: Could not resolve path for " << filePath << endl;
            exit(1);
        }
//        std::cout << filePath << std::endl;
        cv::Mat frame = cv::imread(filePath, cv::IMREAD_ANYDEPTH | cv::IMREAD_ANYCOLOR);
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);

        sdk::SingleFrameOutputs result = pStream->processSingleFrame(
                sdk::RawSourceId::RawSource,
                sdk::SingleFrameOutputType::Detections,
                frame.data,
                frame.cols,
                frame.rows,
                sdk::PixelFormat::RGB8);

        // processing result to detections.csv
        std::vector<sdk::SingleFrameOutput> vResults = result.Data;
        if(vResults.size()){
            for(sdk::SingleFrameOutput& detection : vResults){
                outfile << to_string(fileCounter) << "," << detection.DetectionData.Class.toString() << "," <<
                        to_string(detection.DetectionData.Location.X1) << "," << to_string(detection.DetectionData.Location.Y1) <<
                        "," << to_string(detection.DetectionData.Location.X2) << "," << to_string(detection.DetectionData.Location.Y2) << endl;
            }
        }
        else{
            outfile << to_string(fileCounter) << ",None" << endl;
        }
        std::cout << "\r" << fileCounter << " Out of " << files.size() - 2 << std::flush;
        fileCounter++;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    std::cout << std::endl;
    closedir(directory);

    pStream.reset();
    pPipeline.reset();
}

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
    if (argc != 5)
    {
        std::cerr << "Syntax: ./sdk_sample server_ip frames_folder_path flow_id output_csv_path" << std::endl;
        exit(-1);
    }

    std::string video_path = static_cast<std::string>(argv[2]);

    std::string flow = static_cast<std::string>(argv[3]);
    changeToLower(flow);

    std::string csv_output_path = static_cast<std::string>(argv[4]);

    // Changing ~/ in path to /home/$USER
    size_t pos = csv_output_path.find("~/");
    if(pos != std::string::npos){
        csv_output_path.replace(pos, 2, static_cast<std::string>("/home/") + std::getenv("USER"));
    }

    pos = video_path.find("~/");
    if(pos != std::string::npos){
        video_path.replace(pos, 2, static_cast<std::string>("/home/") + std::getenv("USER"));
    }

//    csv_output_path += "/meerkat_detections.csv";
    outfile.open(csv_output_path);

    try {
        run(argv[1], video_path, flow);
    }
    catch(const std::exception& e){
        std::cout << e.what() << std::endl;
    }
    return 0;
}

