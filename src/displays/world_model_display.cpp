#include <OGRE/OgreSceneNode.h>
#include <OGRE/OgreSceneManager.h>

#include <tf/transform_listener.h>

#include <rviz/visualization_manager.h>
#include <rviz/properties/color_property.h>
#include <rviz/properties/float_property.h>
#include <rviz/properties/int_property.h>
#include <rviz/frame_manager.h>

#include "world_model_display.h"
#include "../visuals/entity_visual.h"

// ----------------------------------------------------------------------------------------------------

float COLORS[27][3] = { { 0.6, 0.6, 0.6},
                        { 0.6, 0.6, 0.4},
                        { 0.6, 0.6, 0.2},
                        { 0.6, 0.4, 0.6},
                        { 0.6, 0.4, 0.4},
                        { 0.6, 0.4, 0.2},
                        { 0.6, 0.2, 0.6},
                        { 0.6, 0.2, 0.4},
                        { 0.6, 0.2, 0.2},
                        { 0.4, 0.6, 0.6},
                        { 0.4, 0.6, 0.4},
                        { 0.4, 0.6, 0.2},
                        { 0.4, 0.4, 0.6},
                        { 0.4, 0.4, 0.4},
                        { 0.4, 0.4, 0.2},
                        { 0.4, 0.2, 0.6},
                        { 0.4, 0.2, 0.4},
                        { 0.4, 0.2, 0.2},
                        { 0.2, 0.6, 0.6},
                        { 0.2, 0.6, 0.4},
                        { 0.2, 0.6, 0.2},
                        { 0.2, 0.4, 0.6},
                        { 0.2, 0.4, 0.4},
                        { 0.2, 0.4, 0.2},
                        { 0.2, 0.2, 0.6},
                        { 0.2, 0.2, 0.4},
                        { 0.2, 0.2, 0.2}
                      };

// ----------------------------------------------------------------------------------------------------

unsigned int djb2(const std::string& str)
{
    int hash = 5381;
    for(unsigned int i = 0; i < str.size(); ++i)
        hash = ((hash << 5) + hash) + str[i]; /* hash * 33 + c */

    if (hash < 0)
        hash = -hash;

    return hash;
}

// ----------------------------------------------------------------------------------------------------

namespace rviz_plugins
{

WorldModelDisplay::WorldModelDisplay()
{
    service_name_property_ = new rviz::StringProperty( "Mesh query service name", "ed/query/meshes", "Service name for querying meshes", this, SLOT( initializeService() ));

    initializeService();
}

void WorldModelDisplay::initializeService()
{
    if (service_client_.exists())
        service_client_.shutdown();

    ros::NodeHandle nh;
    service_client_ = nh.serviceClient<ed_gui_server::QueryMeshes>(service_name_property_->getStdString());
}

void WorldModelDisplay::onInitialize()
{
    MFDClass::onInitialize();
}

WorldModelDisplay::~WorldModelDisplay()
{
}

void WorldModelDisplay::reset()
{
    MFDClass::reset();
}

void WorldModelDisplay::processMessage(const ed_gui_server::EntityInfos::ConstPtr &msg )
{
    // Transform to rviz frame
    Ogre::Quaternion frame_orientation;
    Ogre::Vector3 frame_position;
    if( !context_->getFrameManager()->getTransform( "/map", ros::Time::now(), frame_position, frame_orientation ))
    {
        ROS_DEBUG( "Error transforming from frame '/map' to frame '%s'", qPrintable( fixed_frame_ ));
        return;
    }

    std::vector<std::string> alive_ids;
    for(unsigned int i = 0; i < msg->entities.size(); ++i)
    {
        const ed_gui_server::EntityInfo& info = msg->entities[i];

        if (info.id.size() >= 5 && info.id.substr(info.id.size() - 5) == "floor")
            continue; // Filter floor

        if (!info.has_pose)
            continue;

        if (visuals_.find(info.id) == visuals_.end()) // Visual does not exist yet; create visual
            visuals_[info.id] = boost::shared_ptr<EntityVisual>(new EntityVisual(context_->getSceneManager(), scene_node_));

        boost::shared_ptr<EntityVisual> visual = visuals_[info.id];

        // Get position and orientation
        Ogre::Vector3 position;
        Ogre::Quaternion orientation;
        position.x = info.pose.position.x;
        position.y = info.pose.position.y;
        position.z = info.pose.position.z;

        orientation.x = info.pose.orientation.x;
        orientation.y = info.pose.orientation.y;
        orientation.z = info.pose.orientation.z;
        orientation.w = info.pose.orientation.w;

        visual->setFramePosition( frame_position + position );
        visual->setFrameOrientation( frame_orientation * orientation );

        if (info.mesh_revision > visual->getMeshRevision())
            query_meshes_srv_.request.entity_ids.push_back(info.id); // Mesh
        else if (info.mesh_revision == 0)
            visual->setConvexHull( info.polygon ); // Convex hull

        int i_color = djb2(info.id) % 27;
        visual->setColor ( Ogre::ColourValue(COLORS[i_color][0], COLORS[i_color][1], COLORS[i_color][2], 1.0 ));

        visual->setLabel( info.id );

        alive_ids.push_back(info.id);
    }

    // Check which ids are not present
    std::vector<std::string> ids_to_be_removed;
    for (std::map<std::string, boost::shared_ptr<EntityVisual> >::const_iterator it = visuals_.begin(); it != visuals_.end(); ++it)
    {
        if (std::find(alive_ids.begin(), alive_ids.end(), it->first) == alive_ids.end()) // Not in alive ids
            ids_to_be_removed.push_back(it->first);
    }

    // Remove stale visuals
    for (std::vector<std::string>::const_iterator it = ids_to_be_removed.begin(); it != ids_to_be_removed.end(); ++it)
        visuals_.erase(*it);

    // Perform service call to get missing meshes
    if (!query_meshes_srv_.request.entity_ids.empty())
    {
        if (service_client_.call(query_meshes_srv_))
        {
            for(unsigned int i = 0; i < query_meshes_srv_.response.meshes.size(); ++i)
            {
                const std::string& id = query_meshes_srv_.response.entity_ids[i];
                const ed_gui_server::Mesh& mesh = query_meshes_srv_.response.meshes[i];

                if (visuals_.find(id) == visuals_.end())
                    continue;

                visuals_[id]->setMesh( mesh );
            }
        }
        else
        {
            ROS_ERROR("Could not query for meshes; does the service '%s' exist?", service_name_property_->getStdString().c_str());
        }
    }

    // No more meshes missing :)
    query_meshes_srv_.request.entity_ids.clear();
}

}

#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS(rviz_plugins::WorldModelDisplay,rviz::Display )