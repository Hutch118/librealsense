// License: Apache 2.0. See LICENSE file in root directory.
// Copyright(c) 2016 Intel Corporation. All Rights Reserved.

#include <mutex>
#include <chrono>
#include <vector>
#include <iterator>
#include <cstddef>

#include "device.h"
#include "context.h"
#include "image.h"
#include "metadata-parser.h"

#include "ds5-factory.h"
#include "ds5-private.h"
#include "ds5-options.h"
#include "ds5-timestamp.h"
#include "ds5-rolling-shutter.h"
#include "ds5-active.h"
#include "ds5-color.h"
#include "ds5-motion.h"

namespace librealsense
{
    // PSR
    class rs400_device : public ds5_rolling_shutter, public ds5_advanced_mode_base
    {
    public:
        rs400_device(const platform::backend& backend,
                     const platform::backend_device_group& group)
            : ds5_device(backend, group),
              ds5_rolling_shutter(backend, group),
              ds5_advanced_mode_base(ds5_device::_hw_monitor, get_depth_sensor()) {}
    };

    // ASR
    class rs410_device : public ds5_rolling_shutter,
                         public ds5_active, public ds5_advanced_mode_base
    {
    public:
        rs410_device(const platform::backend& backend,
                     const platform::backend_device_group& group)
            : ds5_device(backend, group),
              ds5_rolling_shutter(backend, group),
              ds5_active(backend, group),
              ds5_advanced_mode_base(ds5_device::_hw_monitor, get_depth_sensor())  {}
    };

    // ASRC
    class rs415_device : public ds5_rolling_shutter,
                         public ds5_active,
                         public ds5_color,
                         public ds5_advanced_mode_base
    {
    public:
        rs415_device(const platform::backend& backend,
                     const platform::backend_device_group& group)
            : ds5_device(backend, group),
              ds5_rolling_shutter(backend, group),
              ds5_active(backend, group),
              ds5_color(backend, group),
              ds5_advanced_mode_base(ds5_device::_hw_monitor, get_depth_sensor())  {}
    };

    // PWGT
    class rs420_mm_device : public ds5_motion, public ds5_advanced_mode_base
    {
    public:
        rs420_mm_device(const platform::backend& backend,
                        const platform::backend_device_group& group)
            : ds5_device(backend, group),
              ds5_motion(backend, group),
              ds5_advanced_mode_base(ds5_device::_hw_monitor, get_depth_sensor())  {}
    };

    // AWG
    class rs430_device : public ds5_active, public ds5_advanced_mode_base
    {
    public:
        rs430_device(const platform::backend& backend,
                     const platform::backend_device_group& group)
            : ds5_device(backend, group),
              ds5_active(backend, group),
              ds5_advanced_mode_base(ds5_device::_hw_monitor, get_depth_sensor())  {}
    };

    // AWGT
    class rs430_mm_device : public ds5_active,
                            public ds5_motion,
                            public ds5_advanced_mode_base
    {
    public:
        rs430_mm_device(const platform::backend& backend,
                        const platform::backend_device_group& group)
            : ds5_device(backend, group),
              ds5_active(backend, group),
              ds5_motion(backend, group),
              ds5_advanced_mode_base(ds5_device::_hw_monitor, get_depth_sensor())  {}
    };

    // AWGC
    class rs435_device : public ds5_active,
                         public ds5_color,
                         public ds5_advanced_mode_base
    {
    public:
        rs435_device(const platform::backend& backend,
                     const platform::backend_device_group& group)
            : ds5_device(backend, group),
              ds5_active(backend, group),
              ds5_color(backend,  group),
              ds5_advanced_mode_base(ds5_device::_hw_monitor, get_depth_sensor()) {}
    };

    // AWGCT
    class rs430_rgb_mm_device : public ds5_active,
                                public ds5_color,
                                public ds5_motion,
                                public ds5_advanced_mode_base
    {
    public:
        rs430_rgb_mm_device(const platform::backend& backend,
                            const platform::backend_device_group& group)
            : ds5_device(backend, group),
              ds5_active(backend, group),
              ds5_color(backend,  group),
              ds5_motion(backend, group),
              ds5_advanced_mode_base(ds5_device::_hw_monitor, get_depth_sensor()) {}
    };

    std::shared_ptr<device_interface> ds5_info::create(const platform::backend& backend) const
    {
        using namespace ds;

        if (_depth.size() == 0) throw std::runtime_error("Depth Camera not found!");
        auto pid = _depth.front().pid;
        platform::backend_device_group group{_depth, _hwm, _hid};

        switch(pid)
        {
        case RS400_PID:
            return std::make_shared<rs400_device>(backend, group);
        case RS410_PID:
            return std::make_shared<rs410_device>(backend, group);
        case RS415_PID:
            return std::make_shared<rs415_device>(backend, group);
        case RS420_PID:
            return std::make_shared<ds5_device>(backend, group);
        case RS420_MM_PID:
            return std::make_shared<rs420_mm_device>(backend, group);
        case RS430_PID:
            return std::make_shared<rs430_device>(backend, group);
        case RS430_MM_PID:
            return std::make_shared<rs430_mm_device>(backend, group);
        case RS430_MM_RGB_PID:
            return std::make_shared<rs430_rgb_mm_device>(backend, group);
        case RS435_RGB_PID:
            return std::make_shared<rs435_device>(backend, group);
        default:
            throw std::runtime_error("Unsupported RS400 model!");
        }
    }

    std::vector<std::shared_ptr<device_info>> ds5_info::pick_ds5_devices(
        std::shared_ptr<platform::backend> backend,
        platform::backend_device_group& group)
    {
        std::vector<platform::uvc_device_info> chosen;
        std::vector<std::shared_ptr<device_info>> results;

        auto valid_pid = filter_by_product(group.uvc_devices, ds::rs4xx_sku_pid);
        auto group_devices = group_devices_and_hids_by_unique_id(group_devices_by_unique_id(valid_pid), group.hid_devices);
        for (auto& g : group_devices)
        {
            auto& devices = g.first;
            auto& hids = g.second;

            if((devices[0].pid == ds::RS430_MM_PID || devices[0].pid == ds::RS420_MM_PID) &&  hids.size()==0)
                continue;

            if (!devices.empty() &&
                mi_present(devices, 0))
            {
                platform::usb_device_info hwm;

                std::vector<platform::usb_device_info> hwm_devices;
                if (ds::try_fetch_usb_device(group.usb_devices, devices.front(), hwm))
                {
                    hwm_devices.push_back(hwm);
                }
                else
                {
                    LOG_DEBUG("try_fetch_usb_device(...) failed.");
                }

                auto info = std::make_shared<ds5_info>(backend, devices, hwm_devices, hids);
                chosen.insert(chosen.end(), devices.begin(), devices.end());
                results.push_back(info);

            }
            else
            {
                LOG_WARNING("DS5 group_devices is empty.");
            }
        }

        trim_device_list(group.uvc_devices, chosen);

        return results;
    }
}
