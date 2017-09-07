#ifndef WORLD_MODEL_DISPLAY_H
#define WORLD_MODEL_DISPLAY_H

#ifndef Q_MOC_RUN
#include <rviz/message_filter_display.h>
#include <ed_gui_server/EntityInfos.h>
#include <ed_gui_server/QueryMeshes.h>
#endif

namespace Ogre
{
class SceneNode;
}

namespace rviz
{
class ColorProperty;
class FloatProperty;
class IntProperty;
}

namespace ed_rviz_plugins
{

class EntityVisual;

class WorldModelDisplay: public rviz::MessageFilterDisplay<ed_gui_server::EntityInfos>
{
Q_OBJECT
public:
    WorldModelDisplay();
    virtual ~WorldModelDisplay();

protected:
    virtual void onInitialize();

    virtual void reset();

private Q_SLOTS:
    void updateProperties();

private:
    void processMessage( const ed_gui_server::EntityInfos::ConstPtr& msg );

    ros::ServiceClient service_client_;
    ed_gui_server::QueryMeshes query_meshes_srv_;

    std::map<std::string, boost::shared_ptr<EntityVisual> > visuals_;

    // User-editable property variables.
    rviz::StringProperty* service_name_property_;
    rviz::FloatProperty* entity_label_opacity_property_;
    rviz::FloatProperty* entity_area_label_opacity_property_;
    rviz::FloatProperty* entity_area_opacity_property_;
};

}

#endif // WORLD_MODEL_DISPLAY_H
