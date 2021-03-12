import open3d as o3d
import numpy as np
import copy
import os
import sys

def draw_registration_result(source, target, transformation):
    source_temp = copy.deepcopy(source)
    target_temp = copy.deepcopy(target)
    #source_temp.paint_uniform_color([1, 0.706, 0])
    #target_temp.paint_uniform_color([0, 0.651, 0.929])
    source_temp.transform(transformation)
    o3d.visualization.draw_geometries([source_temp, target_temp])

def preprocess_point_cloud(pcd, voxel_size):
    print(":: Downsample with a voxel size %.3f." % voxel_size)
    pcd_down = pcd.voxel_down_sample(voxel_size)

    radius_normal = voxel_size * 2
    print(":: Estimate normal with search radius %.3f." % radius_normal)
    pcd_down.estimate_normals(
        o3d.geometry.KDTreeSearchParamHybrid(radius=radius_normal, max_nn=30))

    radius_feature = voxel_size * 5
    print(":: Compute FPFH feature with search radius %.3f." % radius_feature)
    pcd_fpfh = o3d.pipelines.registration.compute_fpfh_feature(
        pcd_down,
        o3d.geometry.KDTreeSearchParamHybrid(radius=radius_feature, max_nn=100))
    return pcd_down, pcd_fpfh
    
   
def prepare_dataset(voxel_size):
    print(":: Load two point clouds")
    source = o3d.io.read_point_cloud("multiway_registration.pcd")  #source is the point cloud that needs to be registered
    
    target = o3d.io.read_point_cloud("/home/bloisi/Downloads/vasca6_p.ply")  #target is the target point cloud
    #target.scale(0.98, source.get_center())
    #trans_init = np.asarray([[0.0, 0.0, 1.0, 0.0], [1.0, 0.0, 0.0, 0.0],
    #                         [0.0, 1.0, 0.0, 0.0], [0.0, 0.0, 0.0, 1.0]])
    #source.transform(trans_init)
    #draw_registration_result(source, target, np.identity(4))

    source_down, source_fpfh = preprocess_point_cloud(source, voxel_size)
    target_down, target_fpfh = preprocess_point_cloud(target, voxel_size)
    return source, target, source_down, target_down, source_fpfh, target_fpfh
    
voxel_size = 0.05  # 0.05 means 5cm for the dataset
source, target, source_down, target_down, source_fpfh, target_fpfh = \
        prepare_dataset(voxel_size)
    
def execute_fast_global_registration(source_down, target_down, source_fpfh,
                                     target_fpfh, voxel_size):
    distance_threshold = voxel_size * 0.2
    print(":: Apply fast global registration with distance threshold %.3f" \
            % distance_threshold)
    result = o3d.pipelines.registration.registration_fast_based_on_feature_matching(
        source_down, target_down, source_fpfh, target_fpfh,
        o3d.pipelines.registration.FastGlobalRegistrationOption(
            maximum_correspondence_distance=distance_threshold))
    return result

result_fast = execute_fast_global_registration(source_down, target_down,
                                               source_fpfh, target_fpfh,
                                               voxel_size)
print(result_fast)
print(result_fast.transformation)
draw_registration_result(source_down, target_down, result_fast.transformation)

threshold = 0.02
print("Apply point-to-point ICP")
reg_p2p = o3d.pipelines.registration.registration_icp(
    source, target, threshold, result_fast.transformation,
    o3d.pipelines.registration.TransformationEstimationPointToPoint())
print(reg_p2p)
print("Transformation is:")
print(reg_p2p.transformation)
draw_registration_result(source, target, reg_p2p.transformation)


coloured = o3d.io.read_point_cloud("multiway_registration.pcd")
#coloured_down = coloured.voxel_down_sample(voxel_size)

#coloured_radius_normal = voxel_size * 2
#coloured_down.estimate_normals(
#        o3d.geometry.KDTreeSearchParamHybrid(radius=coloured_radius_normal, max_nn=30))

#coloured_radius_feature = voxel_size * 5
#coloured_fpfh = o3d.pipelines.registration.compute_fpfh_feature(
#        coloured_down,
#        o3d.geometry.KDTreeSearchParamHybrid(radius=coloured_radius_feature, max_nn=100))

coloured_reg_p2p = o3d.pipelines.registration.registration_icp(
    source, target, threshold, reg_p2p.transformation,
    o3d.pipelines.registration.TransformationEstimationPointToPoint(),
    o3d.pipelines.registration.ICPConvergenceCriteria(max_iteration=2000))
print(coloured_reg_p2p)
print("coloured Transformation is:")
print(coloured_reg_p2p.transformation)
draw_registration_result(coloured, target, reg_p2p.transformation)


def refine_registration(source, target, source_fpfh, target_fpfh, voxel_size):
    distance_threshold = voxel_size * 0.4
    print(":: Point-to-plane ICP registration is applied on original point")
    print("   clouds to refine the alignment. This time we use a strict")
    print("   distance threshold %.3f." % distance_threshold)
    result = o3d.pipelines.registration.registration_icp(
        source, target, distance_threshold, result_ransac.transformation,
        o3d.pipelines.registration.TransformationEstimationPointToPlane())
    return result
    
#result_icp = refine_registration(source, target, source_fpfh, target_fpfh,
                                 #voxel_size)
#print(result_icp)
#draw_registration_result(source, target, result_icp.transformation)

