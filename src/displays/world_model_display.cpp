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

namespace ed_rviz_plugins
{

WorldModelDisplay::WorldModelDisplay()
{
    service_name_property_ = new rviz::StringProperty( "Mesh query service name", "ed/query/meshes", "Service name for querying meshes", this, SLOT( updateProperties() ));

    entity_label_opacity_property_ = new rviz::FloatProperty("Entity label opacity", 1.0, "Opacity of entity label", this, SLOT(updateProperties()));
    entity_area_label_opacity_property_ = new rviz::FloatProperty("Entity Area label opacity", 0.4, "Opacity of entity label", this, SLOT(updateProperties()));
    entity_area_opacity_property_ = new rviz::FloatProperty("Entity Area opacity", 0.2, "Opacity of entity label", this, SLOT(updateProperties()));

    updateProperties();
}

void WorldModelDisplay::updateProperties()
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
  delete service_name_property_;
  delete entity_label_opacity_property_;
  delete entity_area_label_opacity_property_;
  delete entity_area_opacity_property_;
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

        // Set the color
        double r,g,b;
        if (info.color.a != 0) // If a color specified, take color from the info
        {
            r = (float)info.color.r / 255;
            g = (float)info.color.g / 255;
            b = (float)info.color.b / 255;
        }
        else // random color
        {
            int i_color = djb2(info.id) % 27;
            r = COLORS[i_color][0];
            g = COLORS[i_color][1];
            b = COLORS[i_color][2];
        }
        visual->setColor ( Ogre::ColourValue(r, g, b, 1.0f), entity_label_opacity_property_->getFloat(),
                           entity_area_opacity_property_->getFloat(), entity_area_label_opacity_property_->getFloat());

        std::string label;
        label = info.id.substr(0, 6);

        if (!info.type.empty())
            label += " (" + info.type + ")";

        visual->setLabel(label);

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
            for(unsigned int i = 0; i < query_meshes_srv_.response.entity_geometries.size(); ++i)
            {
                const std::string& id = query_meshes_srv_.response.entity_geometries[i].id;

                if (visuals_.find(id) == visuals_.end())
                    continue;

                visuals_[id]->setEntityMeshAndAreas( query_meshes_srv_.response.entity_geometries[i] );
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
PLUGINLIB_EXPORT_CLASS(ed_rviz_plugins::WorldModelDisplay,rviz::Display )
