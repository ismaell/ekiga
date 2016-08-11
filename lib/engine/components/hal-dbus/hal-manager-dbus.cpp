
/*
 * Ekiga -- A VoIP and Video-Conferencing application
 * Copyright (C) 2000-2009 Damien Sandras <dsandras@seconix.com>

 * This program is free software; you can  redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version. This program is distributed in the hope
 * that it will be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Ekiga is licensed under the GPL license and as a special exception, you
 * have permission to link or otherwise combine this program with the
 * programs OPAL, OpenH323 and PWLIB, and distribute the combination, without
 * applying the requirements of the GNU GPL to the OPAL, OpenH323 and PWLIB
 * programs, as long as you do follow the requirements of the GNU GPL for all
 * the rest of the software thus combined.
 */


/*
 *                         hal-manager-dbus.cpp  -  description
 *                         ------------------------------------------
 *   begin                : written in 2008 by Matthias Schneider
 *   copyright            : (c) 2008 by Matthias Schneider
 *   description          : Declaration of the interface of a hal core.
 *                          A hal core manages HalManagers.
 *
 */

#include "hal-manager-dbus.h"
#include "hal-marshal.h"
#include "config.h"

#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>

#ifdef HAVE_V4L
extern "C" {
#include "hal-v4l-helper.h"
}
#endif /* HAVE_V4L */

//FIXME: for tracing
#include "ptlib.h"

HalManager_dbus::HalManager_dbus (Ekiga::ServiceCore & _core)
:    core (_core)
{
  PTRACE(4, "HalManager_dbus\tInitialising HAL Manager");

  GError *error = NULL;
  bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);
  if (error != NULL) {
    PTRACE (1, "HalManager_dbus\tConnecting to system bus failed: " << error->message);
    g_error_free(error);
    return;
  }
  dbus_connection_setup_with_g_main (dbus_g_connection_get_connection (bus), g_main_context_default());

  // Hardware Abstraction Layer registration for callbacks
  hal_proxy = dbus_g_proxy_new_for_name (bus, "org.freedesktop.Hal",
                                              "/org/freedesktop/Hal/Manager",
                                              "org.freedesktop.Hal.Manager");

  dbus_g_proxy_add_signal(hal_proxy, "DeviceRemoved", G_TYPE_STRING, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal(hal_proxy, "DeviceRemoved", G_CALLBACK(&device_removed_cb_proxy), this, NULL);

  dbus_g_proxy_add_signal(hal_proxy, "DeviceAdded", G_TYPE_STRING, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal(hal_proxy, "DeviceAdded", G_CALLBACK(&device_added_cb_proxy), this, NULL);

  populate_devices_list();

  // NetworkManager registration for callbacks
  nm_proxy = dbus_g_proxy_new_for_name (bus, "org.freedesktop.NetworkManager",
                                              "/org/freedesktop/NetworkManager",
                                              "org.freedesktop.NetworkManager");

  dbus_g_proxy_add_signal(nm_proxy, "DeviceNoLongerActive", DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal(nm_proxy, "DeviceNoLongerActive", G_CALLBACK(&interface_no_longer_active_cb_proxy), this, NULL);

  dbus_g_proxy_add_signal(nm_proxy, "DeviceNowActive", DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal(nm_proxy, "DeviceNowActive", G_CALLBACK(&interface_now_active_cb_proxy), this, NULL);

  dbus_g_proxy_add_signal(nm_proxy, "DeviceIP4AddressChange", DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
  dbus_g_proxy_connect_signal(nm_proxy, "DeviceIP4AddressChange", G_CALLBACK(&interface_ip4_address_change_cb_proxy), this, NULL);

  populate_interfaces_list();

  dbus_g_connection_flush (bus);
}

HalManager_dbus::~HalManager_dbus ()
{
  g_object_unref(hal_proxy);
  g_object_unref(nm_proxy);
  dbus_g_connection_unref(bus);
}

void HalManager_dbus::device_added_cb_proxy (DBusGProxy */*object*/, const char *device, gpointer user_data)
{
  HalManager_dbus* hal_manager_dbus = reinterpret_cast<HalManager_dbus *> (user_data);
  hal_manager_dbus->device_added_cb(device);
}

void HalManager_dbus::device_removed_cb_proxy (DBusGProxy */*object*/, const char *device, gpointer user_data)
{
  HalManager_dbus* hal_manager_dbus = reinterpret_cast<HalManager_dbus *> (user_data);
  hal_manager_dbus->device_removed_cb(device);
}

void HalManager_dbus::interface_now_active_cb_proxy (DBusGProxy */*object*/, const char *interface, gpointer user_data)
{
  HalManager_dbus* hal_manager_dbus = reinterpret_cast<HalManager_dbus *> (user_data);
  hal_manager_dbus->interface_now_active_cb(interface);
}

void HalManager_dbus::interface_no_longer_active_cb_proxy (DBusGProxy */*object*/, const char *interface, gpointer user_data)
{
  HalManager_dbus* hal_manager_dbus = reinterpret_cast<HalManager_dbus *> (user_data);
  hal_manager_dbus->interface_no_longer_active_cb(interface);
}

void HalManager_dbus::interface_ip4_address_change_cb_proxy (DBusGProxy */*object*/, const char *interface, gpointer user_data)
{
  HalManager_dbus* hal_manager_dbus = reinterpret_cast<HalManager_dbus *> (user_data);
  hal_manager_dbus->interface_ip4_address_change_cb(interface);
}

void HalManager_dbus::device_added_cb (const char *device)
{
  std::string type, name;
  HalDevice hal_device;
  hal_device.key = device;

  if (!get_device_type_name(device, hal_device))
    return;

  hal_devices.push_back(hal_device);
  PTRACE(4, "HalManager_dbus\tAdded device " << hal_device.category << "," << hal_device.name << "," << hal_device.type << " Video Capabilities: " << hal_device.video_capabilities);

  if (hal_device.category == "alsa") {

    if (hal_device.type == "capture") {
      audioinput_device_added(hal_device.category, hal_device.name);
    }
    else if (hal_device.type == "playback") {
      audiooutput_device_added(hal_device.category, hal_device.name);
    }
  }
  else if (hal_device.category == "oss") {
    audioinput_device_added(hal_device.category, hal_device.name);
    audiooutput_device_added(hal_device.category, hal_device.name);
  }
  else if (hal_device.category == "video4linux") {
      if (hal_device.video_capabilities & 0x01) 
        videoinput_device_added(hal_device.category, hal_device.name, 0x01);
      if (hal_device.video_capabilities & 0x02) 
        videoinput_device_added(hal_device.category, hal_device.name, 0x02);
  }

}

void HalManager_dbus::device_removed_cb (const char *device)
{
  bool found = false;
  std::vector<HalDevice>::iterator iter;

  for (iter = hal_devices.begin ();
       iter != hal_devices.end () ;
       iter++)
      if (iter->key == device) { 
        found = true;
        break;
      }

  if (found) {
    PTRACE(4, "HalManager_dbus\tRemoved device " << iter->category << "," << iter->name << "," << iter->type << " Video Capabilities: " << iter->video_capabilities);

    if (iter->category == "alsa") {
  
      if (iter->type == "capture") {
        audioinput_device_removed(iter->category, iter->name);
      }
      else if (iter->type == "playback") {
        audiooutput_device_removed(iter->category, iter->name);
      }
    }
    else if (iter->category == "oss") {
      audioinput_device_removed(iter->category, iter->name);
      audiooutput_device_removed(iter->category, iter->name);
    }
    else if (iter->category == "video4linux") {
      if (iter->video_capabilities & 0x01) 
        videoinput_device_removed(iter->category, iter->name, 0x01);
      if (iter->video_capabilities & 0x02) 
        videoinput_device_removed(iter->category, iter->name, 0x02);
	
    }


    hal_devices.erase(iter);
  }
}

void HalManager_dbus::interface_now_active_cb (const char *interface)
{
  NmInterface nm_interface;

  nm_interface.key = interface;
  get_interface_name_ip (interface, nm_interface);

  nm_interfaces.push_back(nm_interface);

  PTRACE(4, "HalManager_dbus\tActivated network device " <<  nm_interface.name << "/"<< nm_interface.ip4_address);
  network_interface_up(nm_interface.name, nm_interface.ip4_address);
}

void HalManager_dbus::interface_no_longer_active_cb (const char *interface)
{
  bool found = false;
  std::vector<NmInterface>::iterator iter;

  for (iter = nm_interfaces.begin ();
       iter != nm_interfaces.end () ;
       iter++)
      if (iter->key == interface) { 
        found = true;
        break;
      }

  if (found) {
    PTRACE(4, "HalManager_dbus\tDeactivated network interface " << iter->name << "/" << iter->ip4_address);
    network_interface_down(iter->name, iter->ip4_address);
    nm_interfaces.erase(iter);
  }
}

void HalManager_dbus::interface_ip4_address_change_cb (const char *interface)
{
  PTRACE(4, "HalManager_dbus\tDetected IPv4 address change on network interface " << interface);
  //signal
}

void HalManager_dbus::get_string_property(DBusGProxy *proxy, const char * property, std::string & value)
{
  char* c_value = NULL;
  GError *error = NULL;

  dbus_g_proxy_call (proxy, "GetPropertyString", &error, G_TYPE_STRING, property, G_TYPE_INVALID, G_TYPE_STRING, &c_value, G_TYPE_INVALID);

  if (error != NULL)
    g_error_free(error);
   else
     if (c_value) 
       value = c_value;

  g_free (c_value);
}

bool HalManager_dbus::get_device_type_name (const char * device, HalDevice & hal_device)
{
  DBusGProxy * device_proxy = NULL;
  bool ret = false;

  device_proxy = dbus_g_proxy_new_for_name (bus, "org.freedesktop.Hal",
                                                 device,
                                                 "org.freedesktop.Hal.Device");
  get_string_property(device_proxy, "info.category", hal_device.category);

  hal_device.video_capabilities = 0;

  if (hal_device.category == "alsa") {
    get_string_property(device_proxy, "alsa.card_id", hal_device.name);
    get_string_property(device_proxy, "alsa.type", hal_device.type);
    ret = true;
  }
  else if (hal_device.category == "oss") {
    get_string_property(device_proxy, "oss.card_id", hal_device.name);
    hal_device.type = "";
    ret = true;
  }
  else if (hal_device.category == "video4linux") {
  
#ifdef HAVE_V4L
    std::string device_dir;
    char* v4l1_name;
    char* v4l2_name;
    int supported_versions;

    get_string_property(device_proxy, "video4linux.device", device_dir);

    if (device_dir != "") {
      supported_versions = v4l_get_device_names ((const char*) device_dir.c_str(), &v4l1_name, &v4l2_name);

      if (supported_versions == 0) {
        PTRACE(1, "HalManager_dbus\tNo supported V4L version detected for device " << device_dir);
        hal_device.name = device_dir;
        hal_device.type = "";
      }
      else if (supported_versions == -1) {
        PTRACE(1, "HalManager_dbus\tCould not open device " << device_dir);
        hal_device.name = device_dir;
        hal_device.type = "";
      }
      else {
        if (supported_versions && 1) {
          if (v4l1_name) {
            PTRACE(4, "HalManager_dbus\tDetected V4L capabilities on " << device_dir << " name: " << v4l1_name);
            hal_device.name = v4l1_name;
            hal_device.type = "capture";
            hal_device.video_capabilities  |= V4L_VERSION_1;
          }
          else {
            PTRACE(4, "HalManager_dbus\tSkipped V4L1 device " << device_dir <<  "without name");
            ret = false;
          }
        }
        if (supported_versions && 2) {
          if (v4l2_name) {
            PTRACE(4, "HalManager_dbus\tDetected V4L2 capabilities on " << device_dir << " name: " << v4l2_name);
            hal_device.name = v4l2_name;
            hal_device.type = "capture";
            hal_device.video_capabilities  |= V4L_VERSION_2;
            ret = true;
          }
          else {
            PTRACE(4, "HalManager_dbus\tSkipped V4L2 device " << device_dir <<  "without name");
            ret = false;
          }
        }

      }
  
      v4l_free_device_name(&v4l1_name);
      v4l_free_device_name(&v4l2_name);
    }
#endif /* HAVE_V4L */
  }

  g_object_unref(device_proxy);
  
  // FIXME: Hack to support badly named Logitech devices
  // Maybe this should be fixed in hald?
  if (hal_device.name.substr(0, 17) == "Logitech Logitech") {
    hal_device.name = hal_device.name.substr(9);
  }
  
  return ret;
}

void HalManager_dbus::get_interface_name_ip (const char * interface, NmInterface & nm_interface)
{

  DBusGProxy * interface_proxy = NULL;
  GError *error = NULL;
  gchar* c_value = NULL;
  unsigned address = 0;
  gboolean active = FALSE;

  interface_proxy = dbus_g_proxy_new_for_name (bus, "org.freedesktop.NetworkManager",
                                                 interface,
                                                 "org.freedesktop.NetworkManager.Properties");
  nm_interface.key = interface;

  
// getName
  dbus_g_proxy_call (interface_proxy, "getName", &error, G_TYPE_INVALID, G_TYPE_STRING, &c_value, G_TYPE_INVALID);

  if (error != NULL)
    g_error_free(error);
   else
     if (c_value) 
       nm_interface.name = c_value;

  g_free (c_value);

// getIP4Address
  dbus_g_proxy_call (interface_proxy, "getIP4Address", &error, G_TYPE_INVALID, G_TYPE_UINT, &address, G_TYPE_INVALID);

  if (error != NULL)
    g_error_free(error);
   else {
     gchar* c_address = g_strdup_printf ( "%d.%d.%d.%d", (address >>  0) & 0xff,
                                                         (address >>  8) & 0xff,
  					                 (address >> 16) & 0xff,
					                 (address >> 24) & 0xff);
     nm_interface.ip4_address = c_address;
     g_free (c_address);
   }

// getLinkActive
  dbus_g_proxy_call (interface_proxy, "getLinkActive", &error, G_TYPE_INVALID, G_TYPE_BOOLEAN, &active, G_TYPE_INVALID);

  if (error != NULL)
    g_error_free(error);
   else
      nm_interface.active = active;

  g_object_unref(interface_proxy);
}

void HalManager_dbus::populate_devices_list ()
{
  GError *error = NULL;
  char **device_list;
  char **device_list_ptr;
  HalDevice hal_device;

  PTRACE(4, "HalManager_dbus\tPopulating device list");

  dbus_g_proxy_call (hal_proxy, "GetAllDevices", &error, G_TYPE_INVALID, G_TYPE_STRV, &device_list, G_TYPE_INVALID);

  if (error != NULL) {
    PTRACE(1, "HalManager_dbus\tPopulating full device list failed - " << error->message);
    g_error_free(error);
    return;
  }

  for (device_list_ptr = device_list; *device_list_ptr; device_list_ptr++) {
    hal_device.key = *device_list_ptr;
    
    if (hal_device.key != "/org/freedesktop/Hal/devices/computer") {
      if (get_device_type_name(*device_list_ptr, hal_device)) {
        if ( (hal_device.category == "alsa") ||
             (hal_device.category == "oss") ||
             (hal_device.category == "video4linux") )  
              hal_devices.push_back(hal_device);
      }
    }
  }

  g_strfreev(device_list);

  PTRACE(4, "HalManager_dbus\tPopulated device list with " << hal_devices.size() << " devices");
}

void HalManager_dbus::populate_interfaces_list ()
{
  GError *error = NULL;
  GPtrArray *interface_list;
  NmInterface nm_interface;

  PTRACE(4, "HalManager_dbus\tPopulating interface list");

  dbus_g_proxy_call (nm_proxy, "getDevices", &error, G_TYPE_INVALID,
                     dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_PROXY), &interface_list, G_TYPE_INVALID);

  if (error != NULL) {
    PTRACE(1, "HalManager_dbus\tPopulating full interface list failed - " << error->message);
    g_error_free(error);
    return;
  }

  unsigned i;
  for (i = 0; i < interface_list->len; i++) {

    get_interface_name_ip (dbus_g_proxy_get_path ((DBusGProxy*)g_ptr_array_index (interface_list, i)), nm_interface);
    nm_interfaces.push_back(nm_interface);
  }

  g_ptr_array_free (interface_list, TRUE);

  PTRACE(4, "HalManager_dbus\tPopulated interface list with " << nm_interfaces.size() << " devices");
}
