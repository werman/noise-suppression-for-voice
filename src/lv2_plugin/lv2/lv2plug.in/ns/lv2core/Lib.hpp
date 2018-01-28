/*
  Copyright 2015-2016 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef LV2_LIB_HPP
#define LV2_LIB_HPP

#include "lv2/lv2plug.in/ns/lv2core/Plugin.hpp"

namespace lv2 {

/**
   C++ wrapper for an LV2 plugin library.

   This interface is a convenience for plugin authors only, and is not an ABI
   used by hosts.  Plugin authors should inherit from this interface, passing
   their derived class as the template parameter, and provide an
   lv2_lib_descriptor entry point in C:

   @code
   class MyLib : public lv2::Lib<MyLib> { ... };

   LV2_SYMBOL_EXPORT static const LV2_Lib_Descriptor*
   lv2_lib_descriptor(const char*                bundle_path,
                      const LV2_Feature *const * features)
   {
       return new MyLib(bundle_path, features);
   }
   @endcode
*/
    class Lib : public LV2_Lib_Descriptor
    {
    public:
        Lib(const char* bundle_path, const LV2_Feature*const* features)
        {
            LV2_Lib_Descriptor::handle     = this;
            LV2_Lib_Descriptor::size       = sizeof(LV2_Lib_Descriptor);
            LV2_Lib_Descriptor::cleanup    = s_cleanup;
            LV2_Lib_Descriptor::get_plugin = s_get_plugin;
        }

        virtual ~Lib() {}

        /**
           Plugin accessor, override to return your plugin descriptors.

           Plugins are accessed by index using values from 0 upwards.  Out of range
           indices MUST result in this function returning NULL, so the host can
           enumerate plugins by increasing `index` until NULL is returned.
        */
        virtual const LV2_Descriptor* get_plugin(uint32_t index) { return NULL; }

    private:
        static void s_cleanup(LV2_Lib_Handle handle) {
            delete reinterpret_cast<Lib*>(handle);
        }

        static const LV2_Descriptor* s_get_plugin(LV2_Lib_Handle handle,
                                                  uint32_t       index) {
            return reinterpret_cast<Lib*>(handle)->get_plugin(index);
        }
    };

} /* namespace lv2 */

#endif /* LV2_LIB_HPP */
