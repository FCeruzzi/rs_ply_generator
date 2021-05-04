#ifndef __RS_PLY_GENERATOR__
#define __RS_PLY_GENERATOR__

#include <librealsense2/rs.hpp>
#include <opencv2/opencv.hpp>
#include <librealsense2/hpp/rs_export.hpp>
#include <pcl/io/pcd_io.h>
#include <pcl/point_types.h>
#include <pcl/io/ply_io.h>

#include <boost/thread/thread.hpp>

#include <array>
#ifdef WIN32
#include <experimental/filesystem>
namespace filesystem = std::experimental::filesystem::v1;
#else
#if __has_include(<filesystem>)
#include <filesystem>
namespace filesystem = std::filesystem;
#else
#include <experimental/filesystem>
namespace filesystem = std::experimental::filesystem;
#endif
#endif


class RsPlyGenerator
{
private:
    // RealSense
    rs2::pipeline pipeline;
    rs2::pipeline_profile pipeline_profile;
    rs2::frameset frameset;

    // Color Buffer
    rs2::frame color_frame;
    cv::Mat color_mat;
    uint32_t color_width;
    uint32_t color_height;

    // Depth Buffer
    rs2::frame depth_frame;
    cv::Mat depth_mat;
    uint32_t depth_width;
    uint32_t depth_height;

    // Infrared Buffer
    std::array<rs2::frame, 2> infrared_frames;
    std::array<cv::Mat, 2> infrared_mats;
    uint32_t infrared_width;
    uint32_t infrared_height;

    rs2::align *align_to_color;
    // Declare pointcloud object, for calculating pointclouds and texture mappings
    rs2::pointcloud pc;
    // We want the points object to be persistent so we can display the last cloud when a frame drops
    rs2::points points;

    // Declare depth colorizer for pretty visualization of depth data
    rs2::colorizer color_map;
    cv::Mat depth_colored_mat;
    

    filesystem::path bag_file;
    filesystem::path directory;
    std::vector<int32_t> params;
    bool scaling = false;
    bool display = false;
    
    const char* depth_window_name = "Depth data";
    const char* rgb_window_name = "RGB data";
    

public:
    // Constructor
    RsPlyGenerator(int argc, char* argv[]);

    // Destructor
    ~RsPlyGenerator();

    // Processing
    void run();

private:
    // Initialize
    void initialize( int argc, char * argv[] );

    // Initialize Parameter
    inline void initializeParameter( int argc, char* argv[] );

    // Initialize Sensor
    inline void initializeSensor();

    // Initialize Save
    inline void initializeSave();

    // Finalize
    void finalize();

    // Update Data
    void update();

    // Update Frame
    inline void updateFrame();

    // Update Color
    inline void updateColor();

    // Update Depth
    inline void updateDepth();

    // Update Infrared
    inline void updateInfrared();

    // Draw Data
    void draw();

    // Draw Color
    inline void drawColor();

    // Draw Depth
    inline void drawDepth();

    // Draw Infrared
    inline void drawInfrared();

    // Show Data
    void show();

    // Show Color
    inline void showColor();

    // Show Depth
    inline void showDepth();

    // Show Infrared
    inline void showInfrared();

    // Save Data
    void save();

    // Save Color
    inline void saveColor();

    // Save Depth
    inline void saveDepth();

    // Save Infrared
    inline void saveInfrared();

};

#endif // __RS_PLY_GENERATOR__

