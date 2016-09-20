//-----------------------------------------------------------------------------
//
//	SceneControllerConfiguration.cpp
//
//	Implementation of the Z-Wave COMMAND_CLASS_SCENE_CONTROLLER_CONF
//
//	Copyright (c) 2010 Mal Lansell <openzwave@lansell.org>
//	Copyright (c) 2015 Reagan K. Sanders <vexofp@gmail.com>
//
//	SOFTWARE NOTICE AND LICENSE
//
//	This file is part of OpenZWave.
//
//	OpenZWave is free software: you can redistribute it and/or modify
//	it under the terms of the GNU Lesser General Public License as published
//	by the Free Software Foundation, either version 3 of the License,
//	or (at your option) any later version.
//
//	OpenZWave is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU Lesser General Public License for more details.
//
//	You should have received a copy of the GNU Lesser General Public License
//	along with OpenZWave.  If not, see <http://www.gnu.org/licenses/>.
//
//-----------------------------------------------------------------------------

#include "command_classes/CommandClasses.h"
#include "command_classes/SceneControllerConfiguration.h"
#include "Defs.h"
#include "Msg.h"
#include "Driver.h"
#include "Node.h"
#include "platform/Log.h"

#include "value_classes/ValueByte.h"

using namespace OpenZWave;

enum SceneControllerConfCmd
{
	SceneControllerConfCmd_Set						= 0x01,
	SceneControllerConfCmd_Get						= 0x02,
	SceneControllerConfCmd_Report					= 0x03,
};

const int SceneControllerConfCmd_GroupShift = 1;
const int SceneControllerConfCmd_IndexMask = 0x1;
enum SceneControllerConfIndex
{
  SceneControllerConfIndex_SceneId = 0,
  SceneControllerConfIndex_DimmingDuration
};

SceneControllerConfiguration::SceneControllerConfiguration( uint32 const _homeId, uint8 const _nodeId ) :
  CommandClass( _homeId, _nodeId ),
  m_varsCreated(false)
{
}

//-----------------------------------------------------------------------------
// <SceneControllerConfiguration::RequestState>
// Request current state from the device
//-----------------------------------------------------------------------------
bool SceneControllerConfiguration::RequestState
(
	uint32 const _requestFlags,
	uint8 const _instance,
	Driver::MsgQueue const _queue
)
{
  if( _requestFlags & RequestFlag_Session )
  {
    CreateVars(_instance);

    if(Node* node = GetNodeUnsafe())
    {
      bool out = true;
      for(uint8 i = 1; i <= node->GetNumGroups(); ++i)
      {
        // Doesn't matter which flags we send
        out &= RequestValue( _requestFlags, 
            (i << SceneControllerConfCmd_GroupShift),
            _instance, _queue );
      }

      return out;
    }
  }

	return false;
}

//-----------------------------------------------------------------------------
// <SceneControllerConfiguration::RequestValue>
// Request current value from the device
//-----------------------------------------------------------------------------
bool SceneControllerConfiguration::RequestValue
(
	uint32 const _requestFlags,
	uint8 const _index,
	uint8 const _instance,
	Driver::MsgQueue const _queue
)
{

  uint8 groupId = _index >> SceneControllerConfCmd_GroupShift;

  if ( IsGetSupported() )
  {
    Msg* msg = new Msg( "SceneControllerConfCmd_Get", GetNodeId(), REQUEST, FUNC_ID_ZW_SEND_DATA, true, true, FUNC_ID_APPLICATION_COMMAND_HANDLER, GetCommandClassId() );
    msg->SetInstance( this, _instance );
    msg->Append( GetNodeId() );
    msg->Append( 3 );
    msg->Append( GetCommandClassId() );
    msg->Append( SceneControllerConfCmd_Get );
    msg->Append( groupId );
    msg->Append( GetDriver()->GetTransmitOptions() );
    GetDriver()->SendMsg( msg, _queue );
    return true;
  } else {
    Log::Write(  LogLevel_Info, GetNodeId(), "SceneControllerConfCmd_Get Not Supported on this node");
  }

  return false;
}

//-----------------------------------------------------------------------------
// <SceneControllerConfiguration::HandleMsg>
// Handle a message from the Z-Wave network
//-----------------------------------------------------------------------------
bool SceneControllerConfiguration::HandleMsg
(
	uint8 const* _data,
	uint32 const _length,
	uint32 const _instance	// = 1
)
{
	if( SceneControllerConfCmd_Report == (SceneControllerConfCmd)_data[0] )
	{
    uint8 groupId = _data[1];
    uint8 sceneId = _data[2];
    uint8 dimmingDuration = _data[3];

		Log::Write( LogLevel_Info, GetNodeId(),
        "Received SceneControllerConf report: groupId=%u sceneId=%u dimmingDuration=%u",
        groupId, sceneId, dimmingDuration );

		if( ValueByte* sceneValue = static_cast<ValueByte*>( GetValue( _instance,
            (groupId << SceneControllerConfCmd_GroupShift) | SceneControllerConfIndex_SceneId ) ) )
		{
			sceneValue->OnValueRefreshed( sceneId );
			sceneValue->Release();
		}

		if( ValueByte* dimValue = static_cast<ValueByte*>( GetValue( _instance, 
            (groupId << SceneControllerConfCmd_GroupShift) | SceneControllerConfIndex_DimmingDuration ) ) )
		{
			dimValue->OnValueRefreshed( dimmingDuration );
			dimValue->Release();
		}
		
    return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// <SceneControllerConfiguration::SetValue>
// Set a value on a device
//-----------------------------------------------------------------------------
bool SceneControllerConfiguration::SetValue
(
	Value const& _value
)
{
	uint8 instance = _value.GetID().GetInstance();
  uint8 groupId = _value.GetID().GetIndex() >> SceneControllerConfCmd_GroupShift;
  uint8 index = _value.GetID().GetIndex() & SceneControllerConfCmd_IndexMask;

  if(ValueID::ValueType_Byte != _value.GetID().GetType())
    return false;
  ValueByte const& newValue = static_cast<ValueByte const&>(_value);

  ValueByte *sceneValue, *dimmingValue;
  if( !( sceneValue = static_cast<ValueByte*>( GetValue( instance,
            (groupId << SceneControllerConfCmd_GroupShift) | SceneControllerConfIndex_SceneId ) ) ) )
    return false;
  
  if( !( dimmingValue = static_cast<ValueByte*>( GetValue( instance,
            (groupId << SceneControllerConfCmd_GroupShift) | SceneControllerConfIndex_DimmingDuration ) ) ) )
  {
    sceneValue->Release();
    return false;
  }
  
  bool out = false;

	switch( index )
	{
		case SceneControllerConfIndex_SceneId:
		{
      out = ConfigureGroup(instance, groupId, newValue.GetValue(), dimmingValue->GetValue());
			break;
		}
		case SceneControllerConfIndex_DimmingDuration:
		{
      out = ConfigureGroup(instance, groupId, sceneValue->GetValue(), newValue.GetValue());
			break;
		}
	}
      
  sceneValue->Release();
  dimmingValue->Release();

	return out;
}

//-----------------------------------------------------------------------------
// <SceneControllerConfiguration::ConfigureGroup>
// Configure a group on the scene controller
//-----------------------------------------------------------------------------
bool SceneControllerConfiguration::ConfigureGroup
(
  uint8 const _instance,
  uint8 const _groupId,
  uint8 const _sceneId,
  uint8 const _dimmingDuration
)
{
	Log::Write( LogLevel_Info, GetNodeId(), "SceneControllerConfiguration::ConfigureGroup - group=%u scene=%u duration=%u", _groupId, _sceneId, _dimmingDuration );

	Msg* msg = new Msg( "SceneControllerConfCmd_Set", GetNodeId(), REQUEST, FUNC_ID_ZW_SEND_DATA, true );
	msg->SetInstance( this, _instance );
	msg->Append( GetNodeId() );
  msg->Append( 5 );
  msg->Append( GetCommandClassId() );
  msg->Append( SceneControllerConfCmd_Set );
  msg->Append( _groupId );
  msg->Append( _sceneId );
  msg->Append( _dimmingDuration );
	msg->Append( GetDriver()->GetTransmitOptions() );
	GetDriver()->SendMsg( msg, Driver::MsgQueue_Send );

	return true;
}

//-----------------------------------------------------------------------------
// <SwitchMultilevel::CreateVars>
// Create the values managed by this command class
//-----------------------------------------------------------------------------
void SceneControllerConfiguration::CreateVars
(
	uint8 const _instance
)
{
  Node *node;
	if(!m_varsCreated && (node = GetNodeUnsafe()) )
	{
    for(uint8 i = 1; i <= node->GetNumGroups(); ++i)
    {
      char valueName[32] = "";

      snprintf(valueName, sizeof(valueName), "Button %u Scene Id", i);
      node->CreateValueByte( ValueID::ValueGenre_System, GetCommandClassId(), _instance,
          (i << SceneControllerConfCmd_GroupShift) | SceneControllerConfIndex_SceneId,
          valueName, "", false, false, 0, 0 );

      snprintf(valueName, sizeof(valueName), "Button %u Dimming Duration", i);
      node->CreateValueByte( ValueID::ValueGenre_System, GetCommandClassId(), _instance,
          (i << SceneControllerConfCmd_GroupShift) | SceneControllerConfIndex_DimmingDuration,
          valueName, "", false, false, 0, 0 );

      m_varsCreated = true;
    }
	}
}



