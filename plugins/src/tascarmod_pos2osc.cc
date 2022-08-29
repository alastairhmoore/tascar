/*
 * This file is part of the TASCAR software, see <http://tascar.org/>
 *
 * Copyright (c) 2018 Giso Grimm
 * Copyright (c) 2019 Giso Grimm
 * Copyright (c) 2020 Giso Grimm
 * Copyright (c) 2021 Giso Grimm
 */
/*
 * TASCAR is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, version 3 of the License.
 *
 * TASCAR is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHATABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 3 for more details.
 *
 * You should have received a copy of the GNU General Public License,
 * Version 3 along with TASCAR. If not, see <http://www.gnu.org/licenses/>.
 */

#include "session.h"

class pos2osc_t : public TASCAR::module_base_t {
public:
  pos2osc_t(const TASCAR::module_cfg_t& cfg);
  ~pos2osc_t();
  void update(uint32_t frame, bool running);

private:
  std::string name = "pos2osc";
  std::string url;
  std::string pattern;
  uint32_t mode;
  uint32_t ttl;
  bool transport;
  uint32_t skip;
  uint32_t skipcnt;
  std::string avatar;
  double lookatlen;
  bool triggered;
  bool ignoreorientation;
  bool trigger;
  bool sendsounds;
  bool addparentname;
  float oscale = 1.0f;
  lo_address target;
  std::vector<TASCAR::named_object_t> objects;
  bool bypass = false;
  std::string orientationname = "/headGaze";
};

pos2osc_t::pos2osc_t(const TASCAR::module_cfg_t& cfg)
    : module_base_t(cfg), mode(0), ttl(1), transport(true), skip(0), skipcnt(0),
      lookatlen(1.0), triggered(false), ignoreorientation(false), trigger(true),
      sendsounds(false), addparentname(false)
{
  GET_ATTRIBUTE(name, "", "Default name used in OSC variables");
  GET_ATTRIBUTE_(url);
  GET_ATTRIBUTE_(pattern);
  GET_ATTRIBUTE_(ttl);
  GET_ATTRIBUTE_(mode);
  GET_ATTRIBUTE_BOOL(transport, "Send only while transport is rolling");
  GET_ATTRIBUTE_(avatar);
  GET_ATTRIBUTE(lookatlen, "s", "Duration of look-at animation");
  GET_ATTRIBUTE_BOOL_(triggered);
  GET_ATTRIBUTE_BOOL_(ignoreorientation);
  GET_ATTRIBUTE_BOOL_(sendsounds);
  GET_ATTRIBUTE_BOOL_(addparentname);
  GET_ATTRIBUTE_(skip);
  GET_ATTRIBUTE(oscale, "", "Scaling factor for orientations");
  GET_ATTRIBUTE(orientationname, "", "Name for orientation variables");
  if(url.empty())
    url = "osc.udp://localhost:9999/";
  if(pattern.empty())
    pattern = "/*/*";
  target = lo_address_new_from_url(url.c_str());
  if(!target)
    throw TASCAR::ErrMsg("Unable to create target adress \"" + url + "\".");
  lo_address_set_ttl(target, ttl);
  objects = session->find_objects(pattern);
  if(!objects.size())
    throw TASCAR::ErrMsg("No target objects found (target pattern: \"" +
                         pattern + "\").");
  std::string path = std::string("/") + name;
  if(avatar.size())
    path += "/" + avatar;
  if(mode == 4) {
    cfg.session->add_bool_true(path + "/trigger", &trigger);
    cfg.session->add_bool(path + "/active", &trigger);
    cfg.session->add_bool(path + "/triggered", &triggered);
    cfg.session->add_double(path + "/lookatlen", &lookatlen);
  }
  cfg.session->add_uint(path + "/mode", &mode);
  cfg.session->add_bool(path + "/bypass", &bypass);
  if(triggered)
    trigger = false;
}

pos2osc_t::~pos2osc_t()
{
  lo_address_free(target);
}

void pos2osc_t::update(uint32_t, bool tp_rolling)
{
  if(bypass)
    return;
  if(trigger && ((!triggered && (tp_rolling || (!transport))) || triggered)) {
    if(skipcnt)
      skipcnt--;
    else {
      skipcnt = skip;
      // for(std::vector<TASCAR::named_object_t>::iterator it = obj.begin();
      // it != obj.end(); ++it) {
      for(auto& obj : objects) {
        // copy position from parent object:
        const TASCAR::pos_t& p(obj.obj->c6dof.position);
        TASCAR::zyx_euler_t o(obj.obj->c6dof.orientation);
        if(ignoreorientation)
          o = obj.obj->c6dof_nodelta.orientation;
        std::string path;
        switch(mode) {
        case 0:
          path = obj.name + "/pos";
          lo_send(target, path.c_str(), "fff", p.x, p.y, p.z);
          path = obj.name + "/rot";
          lo_send(target, path.c_str(), "fff", RAD2DEG * o.z * oscale,
                  RAD2DEG * o.y * oscale, RAD2DEG * o.x * oscale);
          break;
        case 1:
          path = obj.name + "/pos";
          lo_send(target, path.c_str(), "ffffff", p.x, p.y, p.z,
                  RAD2DEG * o.z * oscale, RAD2DEG * o.y * oscale,
                  RAD2DEG * o.x * oscale);
          break;
        case 2:
          path = "/tascarpos";
          lo_send(target, path.c_str(), "sffffff", obj.name.c_str(), p.x, p.y,
                  p.z, RAD2DEG * o.z * oscale, RAD2DEG * o.y * oscale,
                  RAD2DEG * o.x * oscale);
          break;
        case 3:
          path = "/tascarpos";
          lo_send(target, path.c_str(), "sffffff", obj.obj->get_name().c_str(),
                  p.x, p.y, p.z, RAD2DEG * o.z * oscale, RAD2DEG * o.y * oscale,
                  RAD2DEG * o.x * oscale);
          if(sendsounds) {
            TASCAR::Scene::src_object_t* src(
                dynamic_cast<TASCAR::Scene::src_object_t*>(obj.obj));
            if(src) {
              std::string parentname(obj.obj->get_name());
              for(const auto& isnd : src->sound) {
                std::string soundname;
                if(addparentname)
                  soundname = parentname + "." + isnd->get_name();
                else
                  soundname = isnd->get_name();
                lo_send(target, path.c_str(), "sffffff", soundname.c_str(),
                        isnd->position.x, isnd->position.y, isnd->position.z,
                        RAD2DEG * isnd->orientation.z * oscale,
                        RAD2DEG * isnd->orientation.y * oscale,
                        RAD2DEG * isnd->orientation.x * oscale);
              }
            }
          }
          break;
        case 4:
          path = "/" + avatar;
          if(lookatlen > 0)
            lo_send(target, path.c_str(), "sffff", "/lookAt", p.x, p.y, p.z,
                    lookatlen);
          else
            lo_send(target, path.c_str(), "sfff", "/lookAt", p.x, p.y, p.z);
          break;
        case 5:
          path = "/" + avatar;
          lo_send(target, path.c_str(), "f", RAD2DEG * o.z * oscale);
          break;
        case 6:
          if(avatar.size())
            path = "/" + avatar;
          else
            path = "/" + obj.obj->get_name();
          lo_send(target, path.c_str(), "sfff", orientationname.c_str(),
                  obj.obj->dorientation.y * oscale,
                  obj.obj->dorientation.z * oscale,
                  obj.obj->dorientation.x * oscale);
          break;
        case 7:
          if(avatar.size())
            path = "/" + avatar;
          else
            path = "/" + obj.obj->get_name();
          lo_send(target, path.c_str(), "sfff", orientationname.c_str(),
                  o.y * oscale, o.z * oscale, o.x * oscale);
          break;
        case 8:
          if(avatar.size())
            path = "/" + avatar;
          else
            path = "/" + obj.obj->get_name();
          lo_send(target, path.c_str(), "fff",
                  RAD2DEG * obj.obj->dorientation.z * oscale,
                  RAD2DEG * obj.obj->dorientation.y * oscale,
                  RAD2DEG * obj.obj->dorientation.x * oscale);
          break;
        case 9:
          if(avatar.size())
            path = "/" + avatar;
          else
            path = "/" + obj.obj->get_name();
          lo_send(target, path.c_str(), "sfff", orientationname.c_str(),
                  obj.obj->dorientation.x * oscale,
                  obj.obj->dorientation.y * oscale,
                  obj.obj->dorientation.z * oscale);
          break;
        case 10:
          if(avatar.size())
            path = "/" + avatar;
          else
            path = "/" + obj.obj->get_name();
          lo_send(target, path.c_str(), "sfff", orientationname.c_str(),
                  obj.obj->dorientation.y * oscale,
                  obj.obj->dorientation.x * oscale,
                  obj.obj->dorientation.z * oscale);
          break;
        }
      }
    }
  }
  if(triggered)
    trigger = false;
}

REGISTER_MODULE(pos2osc_t);

/*
 * Local Variables:
 * mode: c++
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * compile-command: "make -C .."
 * End:
 */
