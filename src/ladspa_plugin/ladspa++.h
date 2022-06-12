/*************************************************************************/
/* ladspa++ - A C++ wrapper for ladspa                                   */
/* Copyright (C) 2014-2018                                               */
/* Johannes Lorenz                                                       */
/* https://github.com/JohannesLorenz/                                    */
/*                                                                       */
/* This program is free software; you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation; either version 3 of the License, or (at */
/* your option) any later version.                                       */
/* This program is distributed in the hope that it will be useful, but   */
/* WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU      */
/* General Public License for more details.                              */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program; if not, write to the Free Software           */
/* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110, USA  */
/*************************************************************************/

#include <tuple>
#include <array>
#include <cassert>
#include <cstdint>

#include "ladspa.h"

namespace ladspa
{

#ifndef DOXYGEN_SHOULD_SKIP_THIS

    namespace helpers
    {

/*
 * sequences
 */

//! default criterium argument, which accepts all numbers
        template<int>
        struct criterium_true {
            static constexpr bool value = true;
        };

//! the resulting sequence type to test for
        template<int ...Seq>
        struct full_seq
        {
        };

        namespace seq_helpers
        {

            template<int Start, int I, template<int> class Criterium, int ...Seq>
            struct _seq_recurse;

//! @a cond is true => spawn @a I into seq and recurse
            template<int Start, int I, template<int> class Criterium, bool cond, int ...Seq>
            struct _seq_check
            {
                using type = typename _seq_recurse<Start, I, Criterium, I, Seq...>::type;
            };

//! @a cond is false => skip @a I in sequence and recurse
            template<int Start, int I, template<int> class Criterium, int ...Seq>
            struct _seq_check<Start, I, Criterium, false, Seq...>
            {
                using type = typename _seq_recurse<Start, I, Criterium, Seq...>::type;
            };

//! counting down, we have not reached the @a Start -> evaluate @a Criterium
            template<int Start, int I, template<int> class Criterium, int ...Seq>
            struct _seq
            {
                using type = typename _seq_check<Start, I, Criterium, Criterium<I>::value, Seq...>::type;
            };

//! counting down, we have reached the @a Start
            template<int Start, template<int> class Criterium, int ...Seq>
            struct _seq<Start, Start, Criterium, Seq...>
            {
                using type = full_seq<Seq...>;
            };

//! sequence recursion, just decrements @a I
            template<int Start, int I, template<int> class Criterium, int ...Seq>
            struct _seq_recurse
            {
                using type = typename _seq<Start, I-1, Criterium, Seq...>::type;
            };

        } // namespace seq_helpers

//! creates maths like range, i.e. [Start, N]
        template<int N, int Start = 1, template<int> class Criterium = criterium_true>
        using math_seq = typename seq_helpers::_seq<Start-1, N, Criterium>::type;

//! creates C like range, i.e. [Start-1, N-1]. Criterium is *not* counted for i-1
        template<int N, int Start = 0, template<int> class Criterium = criterium_true>
        using seq = math_seq<N-1, Start, Criterium>;

        template<class ...Args> static void do_nothing(Args...) {}

/*
 * avoiding instantiations
 */
        template<typename ...> class falsify : std::false_type { };
        template<typename T, T Arg> class falsify_id : std::false_type { };
        template<typename ...Args>
        class dont_instantiate_me {
            static_assert(falsify<Args...>::value, "This should not be instantiated.");
        };
        template<typename T, T Arg>
        class dont_instantiate_me_id {
            static_assert(falsify_id<T, Arg>::value, "This should not be instantiated.");
        };

/*
 * accessing helpers
 */
//! a quick fix since array's data() member is not constexpr
        template<class T, std::size_t N>
        constexpr const T* get_data(const std::array<T, N>& a)
        {
            return &a.front();
        }

        template<class EnumClass>
        constexpr std::size_t enum_size() { return (std::size_t)EnumClass::size; }

//! checks whether class @a T has a ctor with two arguments
//! @note: if we need multiple of those, inherit from a base class
// Source: [1]
        template <typename T>
        class has_ctor_1_args
        {
            struct any
            {
                template<
                        typename U, typename Sfinae =
                        typename std::enable_if< false ==
                                                 std::is_same<U,T>::value, U >::type
                >
                operator U() const;
            };

            template <typename U>
            static int32_t sfinae( decltype( U( any( ) ) ) * );
            template <typename U>
            static int8_t sfinae( ... );

        public:
            static constexpr bool value =
                    sizeof( sfinae<T>( nullptr ) ) == sizeof( int32_t );
        };

        template <class T, template<class > class HaveClass>
        using en_if_has = typename std::enable_if<HaveClass<T>::value>::type;

        template <class T, template<class > class HaveClass>
        using en_if_doesnt_have = typename std::enable_if<!HaveClass<T>::value>::type;

/*
 * misc
 */
        template<class T> struct identity {};

//! if @a Type is the nth integer in @a Others, returns n
        template <std::size_t Type, std::size_t... Others>
        struct id_in_list;

        template <std::size_t Type>
        struct id_in_list<Type>
        {
            static constexpr std::size_t value = -1;
        };

        template <std::size_t Type, std::size_t ... Others>
        struct id_in_list<Type, Type, Others...>
        {
            static constexpr std::size_t value = 0;
        };

        template <std::size_t Type, std::size_t First, std::size_t ... Others>
        struct id_in_list<Type, First, Others...>
        {
            static constexpr std::size_t value
                    = id_in_list<Type, Others...>::value + 1;
        };

    } // namespace helpers

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

//! A struct describing ladspa++'s version
    struct version_t
    {
        char major;
        char minor;
        char patch;
    };

//! This object can be used to receive the current ladspa++ version
    constexpr static version_t version = { 1, 0, 0 };

    typedef unsigned long port_size_t;
    typedef unsigned long sample_size_t;
    typedef unsigned long sample_rate_t;
    typedef unsigned long plugin_index_t;
    typedef LADSPA_Data data;

/**
 * @brief A simple, type safe implementation of a bitmask.
 *
 * Its purpose is to be dependent on a class T, which means that bitmasks from
 * different contexts can not be put into one operator.
 */
    template<class T>
    class bitmask
    {
        int bits;
    public:
        // member functions
        constexpr int get_bits() const { return bits; }
        constexpr bool is(const bitmask<T> property) const {
            return bits & property.bits;
        }
        constexpr bitmask(int _bits = 0) : bits(_bits) {}

        // related functions
        friend constexpr bitmask<T> operator|
                (const bitmask<T>& l, const bitmask<T>& r)
        {
            return bitmask<T>(l.bits|r.bits);
        }
    };

/**
 * @brief Struct containing bits for ladspa properties.
 *
 * The bits specify optional properties of the plugin.
 */
    struct properties
    {
        typedef bitmask<properties> m;
        static constexpr m realtime = LADSPA_PROPERTY_REALTIME;
        static constexpr m inplace_broken = LADSPA_PROPERTY_INPLACE_BROKEN;
        static constexpr m hard_rt_capable = LADSPA_PROPERTY_HARD_RT_CAPABLE;
    };

/**
 * @brief Struct containing bits for ladspa's port types.
 *
 * The bits describe the basic communication type of a port.
 */
    struct port_types
    {
        typedef bitmask<port_types> m;
        // ladspa forbids input + output
        static constexpr m input = LADSPA_PORT_INPUT;
        static constexpr m output = LADSPA_PORT_OUTPUT;
        // ladspa forbids audio + control
        static constexpr m control = LADSPA_PORT_CONTROL;
        static constexpr m audio = LADSPA_PORT_AUDIO;
    };

/**
 * @brief Struct containing bits for ladspa's port hints.
 *
 * The bits give hints about the port's data range.
 */
    struct port_hints
    {
        typedef bitmask<port_hints> m;
        static constexpr m bounded_below = LADSPA_HINT_BOUNDED_BELOW;
        static constexpr m bounded_above = LADSPA_HINT_BOUNDED_ABOVE;
        static constexpr m toggled = LADSPA_HINT_TOGGLED;
        static constexpr m sample_rate = LADSPA_HINT_SAMPLE_RATE;
        static constexpr m logarithmic = LADSPA_HINT_LOGARITHMIC;
        static constexpr m integer = LADSPA_HINT_INTEGER;

        static constexpr m default_mask = LADSPA_HINT_DEFAULT_MASK;
        static constexpr m default_none = LADSPA_HINT_DEFAULT_NONE;
        static constexpr m default_minimum = LADSPA_HINT_DEFAULT_MINIMUM;
        static constexpr m default_low = LADSPA_HINT_DEFAULT_MIDDLE;
        static constexpr m default_high = LADSPA_HINT_DEFAULT_HIGH;
        static constexpr m default_maximum = LADSPA_HINT_DEFAULT_MAXIMUM;
        static constexpr m default_0 = LADSPA_HINT_DEFAULT_0;
        static constexpr m default_1 = LADSPA_HINT_DEFAULT_1;
        static constexpr m default_100 = LADSPA_HINT_DEFAULT_100;
        static constexpr m default_440 = LADSPA_HINT_DEFAULT_440;
    };

/**
 * @brief Class to access a whole buffer of @a T, like std::vector<T>.
 *
 * Can be hard copied, this will be cheap.
 * The class can contain a size information, or not.
 */
    template<class T>
    class buffer_template
    {
        T* _data;
        //! this is overhead for multiple equal-sized buffers
        //! however, this overhead is not much
        std::size_t _size;
    public:
        buffer_template() {}
        buffer_template(T* _in_data, std::size_t _in_size)
                : _data(_in_data), _size(_in_size) {}

        void assign(T* _in_data) { _data = _in_data; }
        void set_size(std::size_t _in_size) { _size = _in_size; }

//	void set_size(std::size_t _in_size) const { return _size; }
        std::size_t size() const { return _size; }

        T& operator[](std::size_t n) { return _data[n]; }
        const T& operator[](std::size_t n) const { return _data[n]; }

        T* begin() { return _data; }
        const T* begin() const { return _data; }
        T* end() { return _data + size(); }
        const T* end() const { return _data + size(); }
        T* data() { return begin(); }
        const T* data() const { return begin(); }
    };

//! A class that behaves like a reference to @a T.
    template<class T>
    class pointer_template
    {
        T* _data;
    public:
        pointer_template() {}
        pointer_template(T* _in_data)
                : _data(_in_data) {}
        pointer_template(T* _in_data, int )
                : pointer_template(_in_data) {}

        void assign(T* _in_data) { _data = _in_data; }
        void set_size(std::size_t _in_size) const {}

        operator const T&() const { return *_data; }
        operator T&() { return *_data; }
    };

//! Class for mutable buffers (like out ports)
    typedef buffer_template<data> buffer;
//! Class for const buffers (like in ports)
    typedef buffer_template<const data> const_buffer;
//! Class for mutable single values (like in ports)
    typedef pointer_template<data> pointer;
//! Class for const single values (like out ports)
    typedef pointer_template<const data> const_pointer;

//! A class which describes a port.
    struct port_info_t
    {
        //! A class which describes the numeric range for a port's values
        struct range_hint_t
        {
            bitmask<port_hints> descriptor;
            data lower_bound;
            data upper_bound;
        };

        enum class type
        {
            name,
            descriptor,
            range_hint
        };

        const char* name; //!< Name of the port. Should be short.
        const char* desription; //!< A short description about what it does
        bitmask<port_types> descriptor; //!< Information about the port type
        range_hint_t hint; //!< Information about numeric range

        constexpr bool is_final() const { return (name == nullptr); }

#ifndef DOXYGEN_SHOULD_SKIP_THIS
        //! This class is used in combination with the get() functions
        template<type I> struct type_id {};

        // get function overloads
        constexpr const char* get(type_id<type::name>) const { return name; }
        constexpr LADSPA_PortDescriptor get(type_id<type::descriptor>) const {
            return descriptor.get_bits();
        }
        constexpr LADSPA_PortRangeHint get(type_id<type::range_hint>) const {
            return { hint.descriptor.get_bits(),
                     hint.lower_bound,
                     hint.upper_bound };
        }
#endif /* DOXYGEN_SHOULD_SKIP_THIS */
        /*
         *  Predefined, common types
         */

//	constexpr const static port_info_t audio_input,
//		audio_output;


/*	constexpr const static port_info_t audio_input = {
	"Input", "Effect's audio input (mono).",
	port_types::input | port_types::audio, {0} };
	constexpr const static port_info_t audio_output = {
	"Output", "Effect's audio output (mono).",
	port_types::output | port_types::audio, {0} };*/
    };

//! A list of very common ports
    namespace port_info_common
    {
        constexpr static port_info_t audio_input = {
                "Input", "Effect's audio input (mono).",
                port_types::input | port_types::audio, {0,0,0} };
        constexpr static port_info_t audio_input_l = {
                "Input (L)", "Effect's audio input (left).",
                port_types::input | port_types::audio, {0,0,0} };
        constexpr static port_info_t audio_input_r = {
                "Input (R)", "Effect's audio input (right).",
                port_types::input | port_types::audio, {0,0,0} };

        constexpr static port_info_t audio_output = {
                "Output", "Effect's audio output (mono).",
                port_types::output | port_types::audio, {0,0,0} };
        constexpr static port_info_t audio_output_l = {
                "Output (L)", "Effect's audio output (mono).",
                port_types::output | port_types::audio, {0,0,0} };
        constexpr static port_info_t audio_output_r = {
                "Output (R)", "Effect's audio output (right).",
                port_types::output | port_types::audio, {0,0,0} };

        // more ports can be added on request...

        //! port marking the end, recognized via the nullptr
        constexpr static port_info_t final_port = { nullptr,
                                                    "Final port (internal use only).", 0, {0,0,0} };
    }

#ifndef DOXYGEN_SHOULD_SKIP_THIS
/*
 *  Return value specialisations for port_array_t
 */
    template<class contained_type, const bitmask<port_types>* bm, class Enable = void>
    struct return_value_base_type
    {
        typedef buffer_template<contained_type> type;
    };

    template<class contained_type, const bitmask<port_types>* bm>
    struct return_value_base_type
            <contained_type, bm,
                    typename std::enable_if<bm->is(port_types::control)>::type>
    {
        typedef pointer_template<contained_type> type;
    };

    template<class base_type,
            const bitmask<port_types>* bm,
            class Enable = void>
    struct return_value_access_type
    {
        typedef const base_type type;
    };

    template<class base_type,
            const bitmask<port_types>* bm>
    struct return_value_access_type
            <base_type, bm,
                    typename std::enable_if<bm->is(port_types::output)>::type>
    {
        typedef base_type type;
    };

    template<class PortNamesT, const port_info_t* PortDesArray>
    class port_array_t;

    template<class port_array_t_t, typename port_array_t_t::port_names_t ...PortIndexes>
    class port_ptrs
    {
        helpers::dont_instantiate_me<port_array_t_t> s;
    };
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief The result of dereferencing a multi_iterator.
 *
 * This class holds multiple pointers to the elements of multiple vectors.
 * The position of these different pointers is always equal.
 * From outside, there a no pointers, but references.
 */
    template<class PortNamesT, const port_info_t* PortDesArray,
            typename port_array_t<PortNamesT, PortDesArray>::port_names_t ...PortIndexes>
    class port_ptrs<port_array_t<PortNamesT, PortDesArray>, PortIndexes...>
    {
        class type_helpers
        {
            template<const bitmask<port_types>* bm>
            using t1 = typename return_value_access_type<data, bm>::type;

        public:
            template<std::size_t PortName>
            class type_at_port
            {
                static constexpr auto arr_elem = PortDesArray[PortName];
                static constexpr auto descr = arr_elem.descriptor;
            public:
                typedef t1<&descr> type; // data or const data
            };

            template<std::size_t ...Is>
            struct _storage_t
            {
                typedef typename std::tuple
                        <typename type_at_port<Is>::type*...> type;
            };
        };
    public:
        template<int PortName>
        using type_at = typename type_helpers::template type_at_port<PortName>::type;

    private:
        typedef typename type_helpers::template _storage_t<(int)PortIndexes...>::type
                storage_t;

        storage_t pointers; //!< valid if we are not at the end()

        typedef port_array_t<PortNamesT, PortDesArray> port_array_t_t;

        template<std::size_t id>
        type_at<id>*& get_ptr() {
            return std::get<helpers::id_in_list<id, (std::size_t)PortIndexes...>::value>(port_ptrs::pointers);
        }

        template<typename port_array_t_t::port_names_t id>
        type_at<(std::size_t)id>*& get_ptr() {
            return get_ptr<(std::size_t)id>();
        }

        template<std::size_t id>
        type_at<id>& get() {
            return *get_ptr<id>();
        }

    public:
        port_ptrs(const port_array_t_t& port_array_t)
                : pointers(port_array_t.template get<PortIndexes>().begin()...) {}
        port_ptrs() {}

        void operator++()
        {
            helpers::do_nothing(++(get_ptr<PortIndexes>())...);
        }

        template<typename port_array_t_t::port_names_t id>
        type_at<(std::size_t)id>& get() {
            return get<(std::size_t)id>();
        }
    };

/**
 * @brief An iterator over a port array container.
 *
 * This class holds multiple pointers to the elements of multiple vectors.
 * The position of these different pointers is always equal.
 */
    template<class port_array_t, typename port_array_t::port_names_t ...PortIndexes>
    class multi_iterator
    {
    private:
        typedef multi_iterator<port_array_t, PortIndexes...> m_type;

        port_ptrs<port_array_t, PortIndexes...> _port_ptrs;
        std::size_t position; //!< valid if we are not at the end()
        const sample_size_t sample_count; //! note: this parameter could be obsolete, or useful...

    public:
        bool operator!=(const m_type& other)
        {
            return position != other.position;
        }

        m_type& operator++()
        {
            ++position;
            ++_port_ptrs;
            return *this;
        }

        port_ptrs<port_array_t, PortIndexes...>& operator*() { return _port_ptrs; }

        //! Iterator pointing to the begin of @a port_array
        multi_iterator(const port_array_t& port_array, sample_size_t _sample_count) :
                _port_ptrs(port_array),
                position(0),
                sample_count(_sample_count)
        {}

        //! Iterator pointing to the end of any port array
        multi_iterator(sample_size_t _sample_count) :
                position(_sample_count),
                sample_count(_sample_count)
        {
        }
    };

/**
 * @brief A container over a port array. Can be iterated.
 *
 * This class is being instantiated to mark
 * over which ports you want to iterate.
 */
    template<class port_array_t_t, typename port_array_t_t::port_names_t ...PortIndexes>
    class samples_container
    {
        const port_array_t_t& port_array_t;
        const sample_size_t sample_count;
        typedef multi_iterator<port_array_t_t, PortIndexes...> multi_itr_type;
    public:
        samples_container(const port_array_t_t& pa, sample_size_t sc)
                : port_array_t(pa), sample_count(sc) {}
        multi_itr_type begin() const {
            return multi_itr_type(port_array_t, sample_count); }
        multi_itr_type end() const { return multi_itr_type(sample_count); }
    };

/**
 * @brief A class that contains all ports.
 *
 * This includes all buffers and the buffer size.
 */
    template<class PortNamesT, const port_info_t* PortDesArray>
    class port_array_t
    {
    private:
        typedef port_array_t<PortNamesT, PortDesArray> m_type;

        class type_helpers
        {

            template<const bitmask<port_types>* bm>
            using t1 = typename return_value_access_type<data, bm>::type;
            template<const bitmask<port_types>* bm>
            using return_value_preparation = typename return_value_base_type<t1<bm>, bm>::type;
        public:
            template<std::size_t PortName>
            class type_at_port
            {
                static constexpr auto arr_elem = PortDesArray[PortName];
                static constexpr auto descr = arr_elem.descriptor;
            public:
                typedef return_value_preparation<&descr> type;
            };

            template<class T> struct _storage_t {
                helpers::dont_instantiate_me<T> x;
            };
            template<int ...Is>
            struct _storage_t<helpers::full_seq<Is...>>
            {
                typedef std::tuple<typename type_at_port<Is>::type...> type;
            };
        };

        constexpr static std::size_t port_size = helpers::enum_size<PortNamesT>();
        constexpr static const port_info_t* port_des_array = PortDesArray;
    public:
        typedef PortNamesT port_names_t;
        template<int PortName>
        using type_at = typename type_helpers::template type_at_port<PortName>::type;
    private:
        typedef typename type_helpers::template _storage_t<
                typename helpers::template seq<port_size>>::type storage_t;
    private:
        /*
         * data
         */
        storage_t storage;
        int _current_sample_count;

        template<int id>
        static void set_static(port_array_t& p, data* d) {
            p.set_internal<id>(d);
        }

        struct caller
        {
            void (&callback)(port_array_t&, data*);
        };

        template<class T>
        static constexpr typename std::array<caller, port_size> init_callers(T)
        {
            static_assert(helpers::falsify<T>::value,
                          "This should not be instantiated.");
            return {}; // constexpr function must have return value
        }

        template<int ...Is>
        static constexpr typename std::array<caller, port_size> init_callers(helpers::full_seq<Is...>)
        {
            return {{{port_array_t::set_static<Is>}...}};
        }

        static constexpr typename std::array<caller, port_size> callers
                = init_callers(typename helpers::seq<port_size>{});

        static constexpr bool in_range_cond(int id)
        {
            return id >= 0 &&
                   (std::size_t)id < helpers::enum_size<port_names_t>();
        }

        template<int id>
        void set_internal(data* d) {
            assert(in_range_cond(id));
            std::get<id>(storage).assign(d);
        }
    public:

#ifndef DOXYGEN_SHOULD_SKIP_THIS
        //! Intended for internal use only
        void set_caller(int id, data* d) {
            callers[id].callback(*this, d);
        }
        //! Intended for internal use only
        void set_current_sample_count(sample_size_t s) {
            _current_sample_count = s;
        }
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/* MSVC does compile the code below */
//        template<std::size_t id>
//        type_at<id> get() const {
//            type_at<id> ret_val = std::get<id>(storage);
//            // buffer size is usually not set - set it
//            ret_val.set_size(_current_sample_count);
//            return ret_val;
//        }
//        template<port_names_t id>
//        type_at<(std::size_t)id> get() const {
//            return get<(std::size_t)id>();
//        }

        template<port_names_t id>
        type_at<(std::size_t)id> get() const {
            type_at<(std::size_t)id> ret_val = std::get<(std::size_t)id>(storage);
            // buffer size is usually not set - set it
            ret_val.set_size(_current_sample_count);
            return ret_val;
        }

        //! lets you choose which buffers you want to iterate over
        template<port_names_t ...port_ids>
        samples_container<m_type, port_ids...> buffers() {
            return samples_container<m_type, port_ids...>(
                    *this, _current_sample_count);
        }

/*	//! lets you iterate over all buffers
	samples_container<m_type, port_ids...> all_buffers() {
		return samples_container<m_type, port_ids...>(
			*this, _current_sample_count);
	}*/

        //! A way the programmer can get the current sample count
        //! in the run() function
        sample_size_t current_sample_count(void) {
            return _current_sample_count;
        }
    };

    template<class PortNamesT, const port_info_t* port_des_array>
    constexpr typename std::array<typename port_array_t<PortNamesT, port_des_array>::caller,
            port_array_t<PortNamesT, port_des_array>::port_size>
            port_array_t<PortNamesT, port_des_array>::callers;

//! A class which the programmer fills in to describe her/his plugin
    struct info_t
    {
        unsigned long unique_id;
        const char * label;
        bitmask<properties> plugin_properties;
        const char * name;
        const char * maker;
        const char * help;
        const char * keywords[8];
        const char * copyright;
        void * implementation_data;
    };

//! returns size of the port array
    static constexpr port_size_t get_port_size(const ladspa::port_info_t* arr) {
        return arr->is_final() ? 0 : get_port_size(arr + 1) + 1;
    }

#ifndef DOXYGEN_SHOULD_SKIP_THIS
/**
 * @brief The direct holder for the plugin class
 *
 * A struct between builder and plugin,
 * in order to hold the port array.
 * (it should not be held by the user)
 *
 * @note Internally, this class is being casted to LADSPA_Handle
 */
    template<class Plugin>
    class plugin_holder_t
    {
        typedef port_array_t<typename Plugin::port_names,
                Plugin::port_info> _port_array_t;

        _port_array_t _ports;
        Plugin plugin;

    public:
        template<class _Plugin, helpers::en_if_has<
                _Plugin, helpers::has_ctor_1_args>* = nullptr>
        plugin_holder_t(helpers::identity<_Plugin>,
                        sample_rate_t _sample_rate
        ) : plugin(_sample_rate) {}

        template<class _Plugin, helpers::en_if_doesnt_have<
                _Plugin, helpers::has_ctor_1_args>* = nullptr>
        plugin_holder_t(helpers::identity<_Plugin>,
                        sample_rate_t
        ) {}

        void run(sample_size_t _sample_count) {
            _ports.set_current_sample_count(_sample_count);
            plugin.run(_ports);
        }

        void connect_port(int _port, data* d) {
            _ports.set_caller(_port, d);
        }
    };
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

/**
 * @brief A class that sets up everything for the C ladpsa side.
 *
 * This includes the static descriptions, port buffers and callbacks.
 * Don't use this class directly, please use the collection class instead.
 */
    template<class Plugin>
    class builder
    {
        typedef plugin_holder_t<Plugin> _plugin_holder_t;

        /*
         * Data
         */
        static constexpr const info_t& descriptor = Plugin::info;
        static constexpr const port_info_t* port_info = Plugin::port_info;
        static constexpr port_size_t port_size = get_port_size(port_info);

        /*
         * This shifts the arrays
         */
        template<port_info_t::type PT, int N, class T, int... Is>
        static constexpr auto _get_elem(helpers::full_seq<Is...>, T* lhs)
        -> std::array<decltype(lhs[0].get(port_info_t::type_id<PT>())), N>
        {
            return {{lhs[Is].get(port_info_t::type_id<PT>())...}};
        }

        template<port_info_t::type PT, int N, class T>
        static constexpr auto get_elem(T* lhs)
        -> decltype( _get_elem<PT, N>(helpers::seq<N>{}, lhs) )
        {
            return _get_elem<PT, N>(helpers::seq<N>{}, lhs);
        }

        /*
         * These functions are the ladspa callbacks
         */

        template<class _Plugin>
        static LADSPA_Handle _instantiate(
                const struct _LADSPA_Descriptor * d, sample_rate_t s) {
            //return new _Plugin;
            return new _plugin_holder_t(helpers::identity<Plugin>(), s);
        }

        static void _cleanup(LADSPA_Handle _instance) {
            delete static_cast<_plugin_holder_t*>(_instance);
        }

        static void
        _connect_port(LADSPA_Handle _instance,
                      port_size_t _port,
                      data * _data_location)
        {
            static_cast<_plugin_holder_t*>(_instance)->
                    connect_port(_port, _data_location);
            //static_cast<_plugin_holder_t*>(_instance)->
            //	_ports.set_caller(_port, _data_location);
        }

/*	template<int i>
	struct criterium_is_buffer
	{
		constexpr static bool value = port_info[i].descriptor.is(port_types::audio);
	};*/

        //void _activate(LADSPA_Handle Instance);
        static void _run(LADSPA_Handle _instance,
                         sample_size_t _sample_count)
        {

            //Plugin* instance = &(static_cast<_plugin_holder_t>(_instance)->plugin);

//		set_sizes(nullptr, _sample_count, helpers::seq<port_size, 0, criterium_is_buffer>{});
            //	_run_with_seq(_instance, _sample_count, helpers::gen_seq<port_size>{});
            //	instance->run(ports);

            static_cast<_plugin_holder_t*>(_instance)->run(_sample_count);
        }

        static constexpr std::array<LADSPA_PortDescriptor, port_size>
                port_descr
                = get_elem<port_info_t::type::descriptor, port_size>(port_info);
        static constexpr std::array<const char*, port_size>
                port_names
                = get_elem<port_info_t::type::name, port_size>(port_info);
        static constexpr std::array<LADSPA_PortRangeHint, port_size>
                port_range_hints
                = get_elem<port_info_t::type::range_hint, port_size>(port_info);
        static constexpr LADSPA_Descriptor descriptor_for_ladspa =
                {
                        descriptor.unique_id,
                        descriptor.label,
                        descriptor.plugin_properties.get_bits(),
                        descriptor.name,
                        descriptor.maker,
                        descriptor.copyright,
                        port_size,
                        helpers::get_data(port_descr),
                        helpers::get_data(port_names),
                        helpers::get_data(port_range_hints),
                        descriptor.implementation_data,
                        _instantiate<Plugin>,
                        _connect_port,
                        nullptr, //_activate,
                        _run,
                        nullptr, // run_adding
                        nullptr, // set_run_adding_gain
                        nullptr, // deactivate
                        _cleanup
                };
    public:
        static const LADSPA_Descriptor& get_ladspa_descriptor()
        { return descriptor_for_ladspa; }
    };

/*
 * Static member definitions
 */
    template<class Plugin>
    constexpr std::array<LADSPA_PortDescriptor,
            builder<Plugin>::port_size>
            builder<Plugin>::port_descr;
    template<class Plugin>
    constexpr std::array<const char*, builder<Plugin>::port_size>
            builder<Plugin>::port_names;
    template<class Plugin>
    constexpr std::array<LADSPA_PortRangeHint, builder<Plugin>::port_size>
            builder<Plugin>::port_range_hints;

    template<class Plugin>
    constexpr LADSPA_Descriptor builder<Plugin>::descriptor_for_ladspa;

//! common strings to be used
    namespace strings
    {
        //! strings for the info_t::copyright field
        namespace copyright
        {
            constexpr const char* none = "None";
            constexpr const char* gpl3 = "GPL v3";
        }
    }

/**
 * @brief This is for a collection of your plugin types.
 *
 * it helps you to select the correct LADSPA descriptor at runtime.
 */
    template<class ...Args>
    class collection
    {
        struct caller
        {
            const LADSPA_Descriptor& (&callback)();
        };

        static constexpr std::array<caller, sizeof...(Args)> init_callers()
        {
            return {{{builder<Args>::get_ladspa_descriptor}...}};
        }

        static constexpr std::array<caller, sizeof...(Args)> callers
                = init_callers();
    public:
        //! Returns the requested descriptor
        //! or nullptr if the index is out of range.
        static const LADSPA_Descriptor* get_ladspa_descriptor(
                plugin_index_t index)
        {
            return (index >= sizeof...(Args))
                   ? nullptr
                   : &callers[index].callback();
        }
    };

    template<class ...Args>
    constexpr std::array<typename collection<Args...>::caller, sizeof...(Args)>
            collection<Args...>::callers;

#ifndef DOXYGEN_SHOULD_SKIP_THIS
    struct correctness_checker
    {
        // TODO: different IDs, In Out, broken inplace etc
    };
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

}

// sources:
// [1] http://stackoverflow.com/questions/16137468/
//     sfinae-detect-constructor-with-one-argument
