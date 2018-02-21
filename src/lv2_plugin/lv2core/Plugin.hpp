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

#ifndef LV2_PLUGIN_HPP
#define LV2_PLUGIN_HPP

#include "lv2core/lv2.h"

namespace lv2 {

/**
   Base interface for plugins.

   This interface is a convenience for plugin authors only, and is not an ABI
   used by hosts.  This is the top of a plugin's class hierarchy, which is
   inherited by extension mixins.  To implement a plugin, inherit from the
   lv2::Plugin template, passing any desired extension mixins as template
   parameters:
   @code
   class MyPlug : public lv2::Plugin<MyPlug> {
       // ...
   };
*/
    class PluginBase {
    public:
        /**
           Instantiate the plugin.

           Note that instance initialisation should generally occur in activate()
           rather than here. If a host calls instantiate(), it MUST call cleanup()
           at some point in the future.

           @param sample_rate Sample rate, in Hz, for the new plugin instance.

           @param bundle_path Path to the LV2 bundle which contains this plugin
           binary. It MUST include the trailing directory separator (e.g. '/') so
           that simply appending a filename will yield the path to that file in the
           bundle.

           @param features A NULL terminated array of LV2_Feature structs which
           represent the features the host supports. Plugins may refuse to
           instantiate if required features are not found here. However, hosts MUST
           NOT use this as a discovery mechanism: instead, use the RDF data to
           determine which features are required and do not attempt to instantiate
           unsupported plugins at all. This parameter MUST NOT be NULL, i.e. a host
           that supports no features MUST pass a single element array containing
           NULL.

           @param valid A pointer to a boolean that must be set to true if the
           plugin instantiates correctly.  The C++ wrappers will free the instance
           and return NULL to the host automatically on failure.

           @return A handle for the new plugin instance, or NULL if instantiation
           has failed.
        */
        PluginBase(double                   sample_rate,
                   const char*              bundle_path,
                   const LV2_Feature*const* features,
                   bool*                    valid)
        {}

        /**
           Clean up plugin instance.

           If activate() was called for a plugin instance then a corresponding call
           to deactivate() MUST be made before the plugin is destroyed.
        */
        virtual ~PluginBase() {}

        /**
           Connect a port on a plugin instance to a memory location.

           Plugin writers should be aware that the host may elect to use the same
           buffer for more than one port and even use the same buffer for both
           input and output (see lv2:inPlaceBroken in lv2.ttl).

           If the plugin has the feature lv2:hardRTCapable then there are various
           things that the plugin MUST NOT do within the connect_port() function;
           see lv2core.ttl for details.

           connect_port() MUST be called at least once for each port before run()
           is called, unless that port is lv2:connectionOptional. The plugin must
           pay careful attention to the block size passed to run() since the block
           allocated may only just be large enough to contain the data, and is not
           guaranteed to remain constant between run() calls.

           connect_port() may be called more than once for a plugin instance to
           allow the host to change the buffers that the plugin is reading or
           writing. These calls may be made before or after activate() or
           deactivate() calls.

           @param port Index of the port to connect. The host MUST NOT try to
           connect a port index that is not defined in the plugin's RDF data. If
           it does, the plugin's behaviour is undefined (a crash is likely).

           @param data_location Pointer to data of the type defined by the port
           type in the plugin's RDF data (e.g. an array of float for an
           lv2:AudioPort). This pointer must be stored by the plugin instance and
           used to read/write data when run() is called. Data present at the time
           of the connect_port() call MUST NOT be considered meaningful.
        */
        virtual void connect_port(uint32_t port, void* data_location) {}

        /**
           Initialise a plugin instance and activate it for use.

           This is separated from instantiate() to aid real-time support and so
           that hosts can reinitialise a plugin instance by calling deactivate()
           and then activate(). In this case the plugin instance MUST reset all
           state information dependent on the history of the plugin instance except
           for any data locations provided by connect_port(). If there is nothing
           for activate() to do then this field may be NULL.

           When present, hosts MUST call this function once before run() is called
           for the first time. This call SHOULD be made as close to the run() call
           as possible and indicates to real-time plugins that they are now live,
           however plugins MUST NOT rely on a prompt call to run() after
           activate().

           The host MUST NOT call activate() again until deactivate() has been
           called first. If a host calls activate(), it MUST call deactivate() at
           some point in the future. Note that connect_port() may be called before
           or after activate().
        */
        virtual void activate() {}

        /**
           Run a plugin instance for a block.

           Note that if an activate() function exists then it must be called before
           run(). If deactivate() is called for a plugin instance then run() may
           not be called until activate() has been called again.

           If the plugin has the feature lv2:hardRTCapable then there are various
           things that the plugin MUST NOT do within the run() function (see
           lv2core.ttl for details).

           As a special case, when `sample_count` is 0, the plugin should update
           any output ports that represent a single instant in time (e.g. control
           ports, but not audio ports). This is particularly useful for latent
           plugins, which should update their latency output port so hosts can
           pre-roll plugins to compute latency. Plugins MUST NOT crash when
           `sample_count` is 0.

           @param sample_count The block size (in samples) for which the plugin
           instance must run.
        */
        virtual void run(uint32_t sample_count) {}

        /**
           Deactivate a plugin instance (counterpart to activate()).

           Hosts MUST deactivate all activated instances after they have been run()
           for the last time. This call SHOULD be made as close to the last run()
           call as possible and indicates to real-time plugins that they are no
           longer live, however plugins MUST NOT rely on prompt deactivation. If
           there is nothing for deactivate() to do then this field may be NULL

           Deactivation is not similar to pausing since the plugin instance will be
           reinitialised by activate(). However, deactivate() itself MUST NOT fully
           reset plugin state. For example, the host may deactivate a plugin, then
           store its state (using some extension to do so).

           Hosts MUST NOT call deactivate() unless activate() was previously
           called. Note that connect_port() may be called before or after
           deactivate().
        */
        virtual void deactivate() {}

        /**
           Return additional plugin data defined by some extenion.

           A typical use of this facility is to return a struct containing function
           pointers to extend the LV2_Descriptor API.

           The actual type and meaning of the returned object MUST be specified
           precisely by the extension. This function MUST return NULL for any
           unsupported URI. If a plugin does not support any extension data, this
           field may be NULL.

           The host is never responsible for freeing the returned value.
        */
        static const void* extension_data(const char* uri) { return NULL; }
    };

/** Empty extension mixin used to terminate mixin list. */
    template<typename Super>
    class NoExtension : public Super {};

/** Plugin mixin list. */
    template<template<class Super> class    First=NoExtension,
            template<class S>     class... Rest>
    struct Plugin : public First< Plugin<Rest...> > {
        Plugin(double r, const char* b, const LV2_Feature* const* f, bool* v)
                : First< Plugin<Rest...> >(r, b, f, v)
        {}
    };

    template<>
    struct Plugin<NoExtension> : public PluginBase {
        Plugin(double r, const char* b, const LV2_Feature* const* f, bool* v)
                : PluginBase(r, b, f, v)
        {}
    };

/**
   Plugin descriptor.

   @code
   static const Descriptor<Amp> amp("http://example.org/amp");
   @endcode
*/
    template<typename PluginType>
    struct Descriptor : public LV2_Descriptor {
        Descriptor(const char* uri) {
            URI            = uri;
            instantiate    = s_instantiate;
            connect_port   = s_connect_port;
            activate       = s_activate;
            run            = s_run;
            deactivate     = s_deactivate;
            cleanup        = s_cleanup;
            extension_data = &PluginType::extension_data;
        }

        static LV2_Handle s_instantiate(const LV2_Descriptor*     descriptor,
                                        double                    rate,
                                        const char*               bundle,
                                        const LV2_Feature* const* features) {
            bool     valid    = true;
            PluginType* instance = new PluginType(rate, bundle, features, &valid);
            if (!valid) {
                delete instance;
                return nullptr;
            }
            return reinterpret_cast<LV2_Handle>(instance);
        }

        static void s_connect_port(LV2_Handle instance, uint32_t port, void* buf) {
            reinterpret_cast<PluginType*>(instance)->connect_port(port, buf);
        }

        static void s_activate(LV2_Handle instance) {
            reinterpret_cast<PluginType*>(instance)->activate();
        }

        static void s_run(LV2_Handle instance, uint32_t sample_count) {
            reinterpret_cast<PluginType*>(instance)->run(sample_count);
        }

        static void s_deactivate(LV2_Handle instance) {
            reinterpret_cast<PluginType*>(instance)->deactivate();
        }

        static void s_cleanup(LV2_Handle instance) {
            delete reinterpret_cast<PluginType*>(instance);
        }
    };

} /* namespace lv2 */

#endif /* LV2_PLUGIN_HPP */