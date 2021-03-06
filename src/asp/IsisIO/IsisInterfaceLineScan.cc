// __BEGIN_LICENSE__
// Copyright (C) 2006-2010 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


// ASP
#include <asp/IsisIO/IsisInterfaceLineScan.h>

using namespace vw;
using namespace asp;
using namespace asp::isis;

// Construct
IsisInterfaceLineScan::IsisInterfaceLineScan( std::string const& filename ) : IsisInterface(filename) {

  // Gutting Isis::Camera
  m_distortmap = m_camera->DistortionMap();
  m_focalmap   = m_camera->FocalPlaneMap();
  m_detectmap  = m_camera->DetectorMap();
  m_alphacube  = new Isis::AlphaCube( m_label );
}

// Custom Function to help avoid over invoking the deeply buried
// functions of Isis::Sensor
void IsisInterfaceLineScan::SetTime( Vector2 const& px, bool calc ) const {
  if ( px != m_c_location ) {
    m_c_location = px;
    m_detectmap->SetParent( m_alphacube->AlphaSample(px[0]),
                            m_alphacube->AlphaLine(px[1]) );

    if ( calc ) {
      // Calculating Spacecraft position and pose
      double ipos[3];
      m_camera->InstrumentPosition(ipos);
      m_center[0] = ipos[0];
      m_center[1] = ipos[1];
      m_center[2] = ipos[2];
      m_center *= 1000;

      std::vector<double> rot_inst = m_camera->InstrumentRotation()->Matrix();
      std::vector<double> rot_body = m_camera->BodyRotation()->Matrix();
      MatrixProxy<double,3,3> R_inst(&(rot_inst[0]));
      MatrixProxy<double,3,3> R_body(&(rot_body[0]));
      m_pose = Quat(R_body*transpose(R_inst));
    }
  }
}

// LMA for projecting point to linescan camera
IsisInterfaceLineScan::EphemerisLMA::result_type
IsisInterfaceLineScan::EphemerisLMA::operator()( IsisInterfaceLineScan::EphemerisLMA::domain_type const& x ) const {

  // Setting Ephemeris Time
  m_camera->SetEphemerisTime( x[0] );

  // Calculating the look direction in camera frame
  double ipos[3];
  m_camera->InstrumentPosition(ipos);
  VectorProxy<double,3> instru(ipos);
  instru *= 1000;  // Spice gives in km
  Vector3 lookB = normalize( m_point - instru );
  std::vector<double> lookB_copy(3);
  lookB_copy[0] = lookB[0];
  lookB_copy[1] = lookB[1];
  lookB_copy[2] = lookB[2];
  std::vector<double> lookJ = m_camera->BodyRotation()->J2000Vector(lookB_copy);
  std::vector<double> lookC = m_camera->InstrumentRotation()->ReferenceVector(lookJ);
  Vector3 look;
  look[0] = lookC[0];
  look[1] = lookC[1];
  look[2] = lookC[2];

  // Projecting to mm focal plane
  look = m_camera->FocalLength() * (look / look[2]);
  m_distortmap->SetUndistortedFocalPlane(look[0], look[1]);
  m_focalmap->SetFocalPlane( m_distortmap->FocalPlaneX(),
                             m_distortmap->FocalPlaneY() );
  result_type result(1);
  // Not exactly sure about lineoffset .. but ISIS does it
  result[0] = m_focalmap->DetectorLineOffset() - m_focalmap->DetectorLine();

  return result;
}

Vector2
IsisInterfaceLineScan::point_to_pixel( Vector3 const& point ) const {

  // First seed LMA with an ephemeris time in the middle of the image
  double middle = lines() / 2;
  m_detectmap->SetParent( 1, m_alphacube->AlphaLine(middle) );
  double start_e = m_camera->EphemerisTime();

  // Build LMA
  EphemerisLMA model( point, m_camera, m_distortmap, m_focalmap );
  int status;
  Vector<double> objective(1), start(1);
  start[0] = start_e;
  Vector<double> solution_e = math::levenberg_marquardt( model,
                                                         start,
                                                         objective,
                                                         status );

  // Make sure we found ideal time
  VW_ASSERT( status > 0,
             MathErr() << " Unable to project point into linescan camera " );

  // Converting now to pixel
  m_camera->SetEphemerisTime( solution_e[0] );

  // Working out pointing
  double ipos[3];
  m_camera->InstrumentPosition(ipos);
  m_center[0] = ipos[0];
  m_center[1] = ipos[1];
  m_center[2] = ipos[2];
  m_center *= 1000;
  Vector3 look = normalize(point-m_center);

  // Calculating Rotation to camera frame
  std::vector<double> rot_inst = m_camera->InstrumentRotation()->Matrix();
  std::vector<double> rot_body = m_camera->BodyRotation()->Matrix();
  MatrixProxy<double,3,3> R_inst(&(rot_inst[0]));
  MatrixProxy<double,3,3> R_body(&(rot_body[0]));
  m_pose = Quat(R_body*transpose(R_inst));

  look = inverse(m_pose).rotate( look );
  look = m_camera->FocalLength() * ( look / look[2] );
  m_distortmap->SetUndistortedFocalPlane( look[0], look[1] );
  m_focalmap->SetFocalPlane( m_distortmap->FocalPlaneX(),
                             m_distortmap->FocalPlaneY() );
  m_detectmap->SetDetector( m_focalmap->DetectorSample(),
                            m_focalmap->DetectorLine() );
  Vector2 pixel( m_detectmap->ParentSample(),
                 m_detectmap->ParentLine() );
  pixel[0] = m_alphacube->BetaSample( pixel[0] );
  pixel[1] = m_alphacube->BetaLine( pixel[1] );
  SetTime( pixel, false );

  pixel -= Vector2(1,1);
  return pixel;
}

Vector3
IsisInterfaceLineScan::pixel_to_vector( Vector2 const& pix ) const {
  Vector2 px = pix + Vector2(1,1);
  SetTime( px, true );

  // Projecting to get look direction
  Vector3 result;
  m_focalmap->SetDetector( m_detectmap->DetectorSample(),
                           m_detectmap->DetectorLine() );
  m_distortmap->SetFocalPlane( m_focalmap->FocalPlaneX(),
                               m_focalmap->FocalPlaneY() );
  result[0] = m_distortmap->UndistortedFocalPlaneX();
  result[1] = m_distortmap->UndistortedFocalPlaneY();
  result[2] = m_distortmap->UndistortedFocalPlaneZ();
  result = normalize( result );
  result = m_pose.rotate(result);
  return result;
}

Vector3
IsisInterfaceLineScan::camera_center( Vector2 const& pix ) const {
  Vector2 px = pix + Vector2(1,1);
  SetTime( px, true );
  return m_center;
}

Quat
IsisInterfaceLineScan::camera_pose( Vector2 const& pix ) const {
  Vector2 px = pix + Vector2(1,1);
  SetTime( px, true );
  return m_pose;
}
