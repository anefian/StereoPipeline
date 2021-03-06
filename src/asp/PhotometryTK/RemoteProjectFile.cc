// __BEGIN_LICENSE__
// Copyright (C) 2006-2010 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__

#include <asp/PhotometryTK/RemoteProjectFile.h>
#include <vw/Core/Exception.h>
#include <vw/Image/PixelTypeInfo.h>
#include <vw/Plate/HTTPUtils.h>
#include <vw/Plate/PlateFile.h>
using namespace vw;
using namespace asp::pho;
using namespace vw::platefile;

namespace asp {
namespace pho {

  // Constructor
  RemoteProjectFile::RemoteProjectFile( Url const& url ) {

    m_url = url;
    std::string path = url.path();
    size_t divider = path.rfind("/");
    m_url.path( path.substr(0,divider) );
    m_projectname = path.substr(divider+1,path.size()-divider-1);

    vw_out(DebugMessage,"ptk") << "Attempting to load ptk server at \""
                               << m_url << "\n";
    m_client = boost::shared_ptr<RpcClient<ProjectService> >(new RpcClient<ProjectService>(m_url));

    // Finding out project ID
    ProjectOpenRequest request;
    vw_out(DebugMessage,"ptk") << "Requesting project: " <<m_projectname<< "\n";
    request.set_name( m_projectname );
    ProjectOpenReply response;
    m_client->OpenRequest( m_client.get(), &request, &response,
                           null_callback() );
    if ( response.project_id() < 0 )
      vw_throw( ArgumentErr() << "Unable to open remote project file. Check url.\n" );
    m_project_id = response.project_id();
  }

  // Error checking macros
#define CHECK_PROJECT_ID()                                      \
  VW_ASSERT( response.project_id() == m_project_id,             \
             IOErr() << "Unable to open remote project file" );
#define CHECK_CAMERA_ID(a)                                              \
  VW_ASSERT( response.camera_id() == a,                                 \
             ArgumentErr() << "Requested unavailable camera ID: " << a );

  // Other public interface
  void RemoteProjectFile::get_project( ProjectMeta& meta ) const {
    ProjectOpenRequest request;
    request.set_name( m_projectname );
    ProjectOpenReply response;
    m_client->OpenRequest( m_client.get(), &request, &response,
                           null_callback() );
    CHECK_PROJECT_ID();
    meta = response.meta();
  }
  void RemoteProjectFile::set_iteration( int32 const& i ) {
    ProjectUpdateRequest request;
    request.set_project_id( m_project_id );
    request.set_iteration( i );
    ProjectUpdateReply response;
    m_client->ProjectUpdate( m_client.get(), &request, &response,
                             null_callback() );
    CHECK_PROJECT_ID();
  }
  int32 RemoteProjectFile::add_camera( CameraMeta const& meta ) {
    CameraCreateRequest request;
    request.set_project_id( m_project_id );
    *(request.mutable_meta()) = meta;
    CameraCreateReply response;
    m_client->CameraCreate( m_client.get(), &request, &response,
                            null_callback() );
    CHECK_PROJECT_ID();
    return response.camera_id();
  }
  void RemoteProjectFile::get_camera( int32 i, CameraMeta& meta ) const {
    CameraReadRequest request;
    request.set_project_id( m_project_id );
    request.set_camera_id( i );
    CameraReadReply response;
    m_client->CameraRead( m_client.get(), &request, &response,
                          null_callback() );
    CHECK_PROJECT_ID();
    CHECK_CAMERA_ID(i);
    meta = response.meta();
  }
  void RemoteProjectFile::set_camera( int32 i, CameraMeta const& meta ) {
    CameraWriteRequest request;
    request.set_project_id( m_project_id );
    request.set_camera_id( i );
    *(request.mutable_meta()) = meta;
    CameraWriteReply response;
    m_client->CameraWrite( m_client.get(), &request, &response,
                           null_callback() );
    CHECK_PROJECT_ID();
    CHECK_CAMERA_ID(i);
  }
  void RemoteProjectFile::get_platefiles( boost::shared_ptr<vw::platefile::PlateFile>& drg,
                                          boost::shared_ptr<vw::platefile::PlateFile>& albedo,
                                          boost::shared_ptr<vw::platefile::PlateFile>& reflect ) const {
    ProjectMeta project_info;
    this->get_project( project_info );

    std::string base_url = m_url.string()+"_index/";
    std::string postfix("?cache_size=1");
    typedef boost::shared_ptr<PlateFile> PlatePtr;
    vw_out(DebugMessage,"ptk") << "Loading platefiles with base url:\n\t"
                               << base_url << "\n";
    drg =
      PlatePtr( new PlateFile(base_url+"DRG.plate"+postfix,
                              project_info.plate_manager(), "", 256, "tif",
                              VW_PIXEL_GRAYA, VW_CHANNEL_UINT8) );
    albedo =
      PlatePtr( new PlateFile(base_url+"Albedo.plate"+postfix,
                              project_info.plate_manager(), "", 256, "tif",
                              VW_PIXEL_GRAYA, VW_CHANNEL_UINT8) );
    if ( project_info.reflectance() != ProjectMeta::NONE ) {
      reflect =
        PlatePtr( new PlateFile(base_url+"Reflectance.plate"+postfix,
                                project_info.plate_manager(), "", 256, "tif",
                                VW_PIXEL_GRAYA, VW_CHANNEL_UINT8) );
    }
  }

}} //end namespace asp::pho
