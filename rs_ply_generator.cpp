#include "rs_ply_generator.hpp"

#include <sstream>
#include <iomanip>
#include <limits>
#include <string>
#include <fstream>
 


// Constructor
RsPlyGenerator::RsPlyGenerator(int argc, char* argv[])
{
    std::cout << "*** RS PLY Generator ***" << std::endl << std::endl;

    // Initialize
    initialize(argc, argv);
}

// Destructor
RsPlyGenerator::~RsPlyGenerator()
{
    // Finalize
    finalize();
}

std::tuple<uint8_t, uint8_t, uint8_t> RsPlyGenerator::get_texcolor(rs2::video_frame texture, rs2::texture_coordinate texcoords){
    const int w = texture.get_width(), h = texture.get_height();
    
    // convert normals [u v] to basic coords [x y]
    int x = std::min(std::max(int(texcoords.u*w + .5f), 0), w - 1);
    int y = std::min(std::max(int(texcoords.v*h + .5f), 0), h - 1);

    int idx = x * texture.get_bytes_per_pixel() + y * texture.get_stride_in_bytes();
    const auto texture_data = reinterpret_cast<const uint8_t*>(texture.get_data());
    return std::tuple<uint8_t, uint8_t, uint8_t>(texture_data[idx], texture_data[idx+1], texture_data[idx+2]);
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr RsPlyGenerator::points_to_pcl(const rs2::points& points, const rs2::video_frame& color, cv::Mat& filtered){
        
    auto sp = points.get_profile().as<rs2::video_stream_profile>();
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
    
    // Config of PCL Cloud object
    cloud->width = static_cast<uint32_t>(sp.width());//information about sensor generated pointcloud to initialize filtered pointcloud
    cloud->height = static_cast<uint32_t>(sp.height());
    cloud->is_dense = false;
    cloud->points.resize(points.size());//to adjust to points number

    auto tex_coords = points.get_texture_coordinates();// useful to get the color in the get_text_color function
    auto vertices = points.get_vertices();// vertices = points of the pointcloud returned by sensor

    int idx = 0;//useful for sensor pointcloud scan, because vertices is a one-dimensional array
    for(int i = 0; i < cloud->height; ++i){
      for(int j = 0; j < cloud->width; ++j){//mask scan
        double c = (double)filtered.at<uchar>(i,j);//take mask pixel value, because mask have only one channel is a char
        if(c >= 60){//>=60 is a threshold for the mask because from 512 to 1280 the resize blurs a bit
          //if it belongs it take its point inside sensor pointcloud and put it in the new one
          cloud->points[idx].x = vertices[idx].x;
          cloud->points[idx].y = vertices[idx].y;
          cloud->points[idx].z = vertices[idx].z;

          //from here it take colors
          std::tuple<uint8_t, uint8_t, uint8_t> current_color;
          //extract color from original pointcloud
          current_color = get_texcolor(color, tex_coords[idx]);

          cloud->points[idx].r = std::get<0>(current_color);
          cloud->points[idx].g = std::get<1>(current_color);
          cloud->points[idx].b = std::get<2>(current_color);
        }
        idx++;
      }
    }
   return cloud;
}

// Processing
void RsPlyGenerator::run()
{
    // Retrieve Last Position
    uint64_t last_position = pipeline_profile.get_device().as<rs2::playback>().get_position();


    int period_in_frames = 100;
    int frame_cnt = 0;  
    std::string pc_idx = "0";
    //int cont = 0;
    // Main Loop
    while(true){
        // Update Data
        update();

        // Draw Data
        draw();

        // Show Data
        if( display ){
          show();
        }
          
        if(frame_cnt == period_in_frames) {
            pc.map_to(color_frame);
            points = pc.calculate(depth_frame);

            //Edited part
            cv::Mat dst_rgb;
            //Save color frame
            cv::Mat color = cv::Mat( color_frame.as<rs2::video_frame>().get_height(), color_frame.as<rs2::video_frame>().get_width(), CV_8UC3, const_cast<void*>( color_frame.get_data() ) ).clone();
            cv::resize(color, dst_rgb, cv::Size(512,512));//for inference
            cv::imwrite("./segnet/image.png", dst_rgb);
            std::cout << "Python" << std::endl;
            //Inference
            system("python3 ./segnet/test.py --save_dir ./segnet/mask/ --test_list ./segnet/to_mask.txt --resume ./model.hdf5");
            system("python3 ./segnet/inference.py");
            //Load mask
            cv::Mat mask = cv::imread("./segnet/mask/image.png", cv::IMREAD_GRAYSCALE); 
            cv::Mat mask_res;
            int newW = 1280;
            int newH = 720;
            //Mask resize
            cv::resize(mask, mask_res, cv::Size(newW,newH));
            //Building pointcloud
            pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud = points_to_pcl(points, color_frame, mask_res);
            
            float phi = M_PI;
            Eigen::Affine3f transform = Eigen::Affine3f::Identity();
            transform.rotate (Eigen::AngleAxisf (phi, Eigen::Vector3f::UnitX()));
            pcl::transformPointCloud(*cloud, *cloud, transform);

            pcl::io::savePLYFileASCII("./pointcloud_" + pc_idx + ".ply", *cloud);
            pc_idx = std::to_string(1+std::stoi(pc_idx));
            frame_cnt = 0;
        }else {
            frame_cnt++;
        }
        
        // Key Check
        const int32_t key = cv::waitKey(3);
        if( key == 'q' || key == 27){
            break;
        }

        // End of Position
        const uint64_t current_position = pipeline_profile.get_device().as<rs2::playback>().get_position();
        if( static_cast<int64_t>( current_position - last_position ) < 0 ){
            break;
        }
        last_position = current_position;
    }
}

// Initialize
void RsPlyGenerator::initialize( int argc, char * argv[] )
{
    cv::namedWindow(depth_window_name, cv::WINDOW_AUTOSIZE);
    cv::setUseOptimized(true);

    // Initialize Parameter
    initializeParameter(argc, argv);

    // Initialize Sensor
    initializeSensor();

    align_to_color = new rs2::align(RS2_STREAM_COLOR);
}

// Initialize Parameter
inline void RsPlyGenerator::initializeParameter( int argc, char * argv[] )
{
    // Create Command Line Parser
    const std::string keys =
        "{ help h    |       | print this message.                                                      }"
        "{ bag b     |       | path to input bag file. (required)                                       }"
        "{ scaling s | false | enable depth scaling for visualization. false is raw 16bit image. (bool) }"
        "{ quality q | 95    | jpeg encoding quality for color and infrared. [0-100]                    }"
        "{ display d | false | display each stream images on window. false is not display. (bool)       }";
    cv::CommandLineParser parser( argc, argv, keys );

    if( parser.has( "help" ) ){
        parser.printMessage();
        std::exit( EXIT_SUCCESS );
    }

    // Check Parsing Error
    if(!parser.check()){
        parser.printErrors();
        throw std::runtime_error( "failed command arguments" );
    }

    // Retrieve Bag File Path (Required)
    if(!parser.has( "bag" ) ){
        throw std::runtime_error( "failed can't find input bag file" );
    }
    else{
        bag_file = parser.get<cv::String>("bag").c_str();
        if( !filesystem::is_regular_file(bag_file)){
            throw std::runtime_error("input bag file is not regular");
        }
        else if(bag_file.extension() != ".bag"){
            throw std::runtime_error("wrong input bag file extension");
        }
    }

    // Retrieve Scaling Flag (Option)
    if(!parser.has("scaling")){
        scaling = false;
    }
    else{
        scaling = parser.get<bool>( "scaling" );
    }

    // Retrieve JPEG Quality (Option)
    if( !parser.has( "quality" ) ){
        params = { cv::IMWRITE_JPEG_QUALITY, 95 };
    }
    else{
        params = { cv::IMWRITE_JPEG_QUALITY, std::min( std::max( 0, parser.get<int32_t>( "quality" ) ), 100 ) };
    }

    // Retrieve Display Flag (Option)
    if( !parser.has( "display" ) ){
        display = false;
    }
    else{
        display = parser.get<bool>( "display" );
    }
}

// Initialize Sensor
inline void RsPlyGenerator::initializeSensor()
{
    // Retrieve Each Streams that contain in File
    rs2::config config;
    rs2::context context;
    const rs2::playback playback = context.load_device( bag_file.string() );
    const std::vector<rs2::sensor> sensors = playback.query_sensors();
    for( const rs2::sensor& sensor : sensors ){
        const std::vector<rs2::stream_profile> stream_profiles = sensor.get_stream_profiles();
        for( const rs2::stream_profile& stream_profile : stream_profiles ){
            config.enable_stream( stream_profile.stream_type(), stream_profile.stream_index() );
        }
    }

    // Start Pipeline
    config.enable_device_from_file(playback.file_name());
    pipeline_profile = pipeline.start(config);

    // Set Non Real Time Playback
    pipeline_profile.get_device().as<rs2::playback>().set_real_time( false );

    // Show Enable Streams
    std::cout << "Available Streams" << std::endl;
    const std::vector<rs2::stream_profile> stream_profiles = pipeline_profile.get_streams();
    for( const rs2::stream_profile stream_profile : stream_profiles ) {
        std::cout << stream_profile.stream_name() << std::endl;
    }
}

// Initialize Save
inline void RsPlyGenerator::initializeSave()
{
    // Create Root Directory (Bag File Name)
    directory = bag_file.parent_path().generic_string() + "/" + bag_file.stem().string();
    if( !filesystem::create_directories( directory ) ){
        throw std::runtime_error( "failed can't create root directory" );
    }

    // Create Sub Directory for Each Streams (Stream Name)
    const std::vector<rs2::stream_profile> stream_profiles = pipeline_profile.get_streams();
    for( const rs2::stream_profile stream_profile : stream_profiles ){
        filesystem::path sub_directory = directory.generic_string() + "/" + stream_profile.stream_name();
        filesystem::create_directories( sub_directory );
    }
}

// Finalize
void RsPlyGenerator::finalize()
{
    // Close Windows
    cv::destroyAllWindows();

    // Stop Pipline
    pipeline.stop();
}

// Update Data
void RsPlyGenerator::update()
{
    // Update Frame
    updateFrame();    

    // Update Color
    updateColor();

    // Update Depth
    updateDepth();    

    // Update Infrared
    //updateInfrared();

}

// Update Frame
inline void RsPlyGenerator::updateFrame()
{
    // Update Frame
    frameset = pipeline.wait_for_frames();

    // Align all frames to color viewport
    frameset = align_to_color->process(frameset);
    
}

// Update Color
inline void RsPlyGenerator::updateColor()
{
    // Retrieve Color Flame
    color_frame = frameset.get_color_frame();
    if( !color_frame ){
        return;
    }

    // Retrive Frame Size
    color_width = color_frame.as<rs2::video_frame>().get_width();
    color_height = color_frame.as<rs2::video_frame>().get_height();
}

// Update Depth
inline void RsPlyGenerator::updateDepth()
{
    // Retrieve Depth Flame
    depth_frame = frameset.get_depth_frame();
    
    if( !depth_frame ){
        return;
    }

    // Retrive Frame Size
    depth_width = depth_frame.as<rs2::video_frame>().get_width();
    depth_height = depth_frame.as<rs2::video_frame>().get_height();
}

// Update Infrared
inline void RsPlyGenerator::updateInfrared()
{
    // Retrieve Infrared Flames
    frameset.foreach_rs( [this]( const rs2::frame& frame ){
        if( frame.get_profile().stream_type() == rs2_stream::RS2_STREAM_INFRARED ){
            infrared_frames[frame.get_profile().stream_index() - 1] = frame;
        }
    } );

    const rs2::frame& infrared_frame = infrared_frames.front();
    if( !infrared_frame ){
        return;
    }

    // Retrive Frame Size
    infrared_width = infrared_frame.as<rs2::video_frame>().get_width();
    infrared_height = infrared_frame.as<rs2::video_frame>().get_height();
}

// Draw Data
void RsPlyGenerator::draw()
{
    // Draw Color
    drawColor();

    // Draw Depth
    drawDepth();

    // Draw Infrared
    //drawInfrared();
}

// Draw Color
inline void RsPlyGenerator::drawColor()
{
    if( !color_frame ){
        return;
    }

    // Create cv::Mat form Color Frame
    const rs2_format color_format = color_frame.get_profile().format();
    switch( color_format ){
        // RGB8
        case rs2_format::RS2_FORMAT_RGB8:
        {
            color_mat = cv::Mat( color_height, color_width, CV_8UC3, const_cast<void*>( color_frame.get_data() ) ).clone();
            cv::cvtColor( color_mat, color_mat, cv::COLOR_RGB2BGR );
            break;
        }
        // RGBA8
        case rs2_format::RS2_FORMAT_RGBA8:
        {
            color_mat = cv::Mat( color_height, color_width, CV_8UC4, const_cast<void*>( color_frame.get_data() ) ).clone();
            cv::cvtColor( color_mat, color_mat, cv::COLOR_RGBA2BGRA );
            break;
        }
        // BGR8
        case rs2_format::RS2_FORMAT_BGR8:
        {
            color_mat = cv::Mat( color_height, color_width, CV_8UC3, const_cast<void*>( color_frame.get_data() ) ).clone();
            break;
        }
        // BGRA8
        case rs2_format::RS2_FORMAT_BGRA8:
        {
            color_mat = cv::Mat( color_height, color_width, CV_8UC4, const_cast<void*>( color_frame.get_data() ) ).clone();
            break;
        }
        // Y16 (GrayScale)
        case rs2_format::RS2_FORMAT_Y16:
        {
            color_mat = cv::Mat( color_height, color_width, CV_16UC1, const_cast<void*>( color_frame.get_data() ) ).clone();
            constexpr double scaling = static_cast<double>( std::numeric_limits<uint8_t>::max() ) / static_cast<double>( std::numeric_limits<uint16_t>::max() );
            color_mat.convertTo( color_mat, CV_8U, scaling );
            break;
        }
        // YUYV
        case rs2_format::RS2_FORMAT_YUYV:
        {
            color_mat = cv::Mat( color_height, color_width, CV_8UC2, const_cast<void*>( color_frame.get_data() ) ).clone();
            cv::cvtColor( color_mat, color_mat, cv::COLOR_YUV2BGR_YUYV );
            break;
        }
        default:
            throw std::runtime_error( "unknown color format" );
            break;
    }
}

// Draw Depth
inline void RsPlyGenerator::drawDepth()
{
    if( !depth_frame ){
        return;
    }

    const void* data = depth_frame.get_data();
    // Create cv::Mat form Depth Frame
    depth_mat = cv::Mat(depth_height, depth_width, CV_16UC1, (void*)data).clone();
    
    // Create OpenCV matrix of size (w,h) from the colorized depth data
    const void* color_data = (depth_frame.apply_filter(color_map)).get_data();
    //depth_colored_mat = cv::Mat(cv::Size(depth_width, depth_height), cv::CV_8UC3, data, cv::Mat::AUTO_STEP);
    depth_colored_mat = cv::Mat(depth_height, depth_width, CV_8UC3, (void*)color_data);
}

// Draw Infrared
inline void RsPlyGenerator::drawInfrared()
{
    // Create cv::Mat form Infrared Frame
    for( const rs2::frame& infrared_frame : infrared_frames ){
        if( !infrared_frame ){
            continue;
        }

        const uint8_t infrared_stream_index = infrared_frame.get_profile().stream_index();
        const uint8_t infrared_mat_index = infrared_stream_index - 1;
        const rs2_format infrared_format = infrared_frame.get_profile().format();
        switch( infrared_format ){
            // RGB8 (Color)
            case rs2_format::RS2_FORMAT_RGB8:
            {
                infrared_mats[infrared_mat_index] = cv::Mat( infrared_height, infrared_width, CV_8UC3, const_cast<void*>( infrared_frame.get_data() ) ).clone();
                cv::cvtColor( infrared_mats[infrared_mat_index], infrared_mats[infrared_mat_index], cv::COLOR_RGB2BGR );
                break;
            }
            // RGBA8 (Color)
            case rs2_format::RS2_FORMAT_RGBA8:
            {
                infrared_mats[infrared_mat_index] = cv::Mat( infrared_height, infrared_width, CV_8UC4, const_cast<void*>( infrared_frame.get_data() ) ).clone();
                cv::cvtColor( infrared_mats[infrared_mat_index], infrared_mats[infrared_mat_index], cv::COLOR_RGBA2BGRA );
                break;
            }
            // BGR8 (Color)
            case rs2_format::RS2_FORMAT_BGR8:
            {
                infrared_mats[infrared_mat_index] = cv::Mat( infrared_height, infrared_width, CV_8UC3, const_cast<void*>( infrared_frame.get_data() ) ).clone();
                break;
            }
            // BGRA8 (Color)
            case rs2_format::RS2_FORMAT_BGRA8:
            {
                infrared_mats[infrared_mat_index] = cv::Mat( infrared_height, infrared_width, CV_8UC4, const_cast<void*>( infrared_frame.get_data() ) ).clone();
                break;
            }
            // Y8
            case rs2_format::RS2_FORMAT_Y8:
            {
                infrared_mats[infrared_mat_index] = cv::Mat( infrared_height, infrared_width, CV_8UC1, const_cast<void*>( infrared_frame.get_data() ) ).clone();
                break;
            }
            // UYVY
            case rs2_format::RS2_FORMAT_UYVY:
            {
                infrared_mats[infrared_mat_index] = cv::Mat( infrared_height, infrared_width, CV_8UC2, const_cast<void*>( infrared_frame.get_data() ) ).clone();
                cv::cvtColor( infrared_mats[infrared_mat_index], infrared_mats[infrared_mat_index], cv::COLOR_YUV2GRAY_UYVY );
                break;
            }
            default:
                throw std::runtime_error( "unknown infrared format" );
                break;
        }
    }
}

// Show Data
void RsPlyGenerator::show()
{
    // Show Color
    showColor();

    // Show Depth
    showDepth();

    // Show Infrared
    //showInfrared();
}

// Show Color
inline void RsPlyGenerator::showColor()
{
    if( !color_frame ){
        return;
    }

    if( color_mat.empty() ){
        return;
    }

    // Show Color Image
    cv::imshow( rgb_window_name, color_mat );
}

// Show Depth
inline void RsPlyGenerator::showDepth()
{
    if( !depth_frame ){
        return;
    }

    if( depth_mat.empty() ){
        return;
    }

    // Scaling
    //cv::Mat scale_mat;
    //depth_mat.convertTo( scale_mat, CV_8U, -255.0 / 10000.0, 255.0 ); // 0-10000 -> 255(white)-0(black)

    // Show Depth Image
    cv::imshow( depth_window_name, depth_colored_mat );
}

// Show Infrared
inline void RsPlyGenerator::showInfrared()
{
    for( const rs2::frame& infrared_frame : infrared_frames ){
        if( !infrared_frame ){
            continue;
        }

        const uint8_t infrared_stream_index = infrared_frame.get_profile().stream_index();
        const uint8_t infrared_mat_index = infrared_stream_index - 1;
        if( infrared_mats[infrared_mat_index].empty() ){
            continue;
        }

        // Show Infrared Image
        //cv::imshow( "Infrared " + std::to_string( infrared_stream_index ), infrared_mats[infrared_mat_index] );
    }
}

// Save Data
void RsPlyGenerator::save()
{
    // Save Color
    saveColor();

    // Save Depth
    saveDepth();

    // Save Infrared
    saveInfrared();
}

// Save Color
inline void RsPlyGenerator::saveColor()
{
    if( !color_frame ){
        return;
    }

    if( color_mat.empty() ){
        return;
    }

    // Create Save Directory and File Name
    std::ostringstream oss;
    oss << directory.generic_string() << "/Color/";
    oss << std::setfill( '0' ) << std::setw( 6 ) << color_frame.get_frame_number() << ".jpg";

    // Write Color Image
    cv::imwrite( oss.str(), color_mat, params );
}

// Save Depth
inline void RsPlyGenerator::saveDepth()
{
    if( !depth_frame ){
        return;
    }

    if( depth_mat.empty() ){
        return;
    }

    // Create Save Directory and File Name
    std::ostringstream oss;
    oss << directory.generic_string() << "/Depth/";
    oss << std::setfill( '0' ) << std::setw( 6 ) << depth_frame.get_frame_number() << ".png";

    // Scaling
    cv::Mat scale_mat = depth_mat;
    if( scaling ){
        depth_mat.convertTo( scale_mat, CV_8U, -255.0 / 10000.0, 255.0 ); // 0-10000 -> 255(white)-0(black)
    }

    // Write Depth Image
    cv::imwrite( oss.str(), scale_mat );
}

// Save Infrared
inline void RsPlyGenerator::saveInfrared()
{
    for( const rs2::frame& infrared_frame : infrared_frames ){
        if( !infrared_frame ){
            continue;
        }

        const uint8_t infrared_stream_index = infrared_frame.get_profile().stream_index();
        const uint8_t infrared_mat_index = infrared_stream_index - 1;
        if( infrared_mats[infrared_mat_index].empty() ){
            continue;
        }

        // Create Save Directory and File Name
        std::ostringstream oss;
        oss << directory.generic_string() << "/Infrared " << std::to_string( infrared_stream_index ) << "/";
        oss << std::setfill( '0' ) << std::setw( 6 ) << infrared_frame.get_frame_number() << ".jpg";

        // Write Infrared Image
        cv::imwrite( oss.str(), infrared_mats[infrared_mat_index], params );
    }
}


