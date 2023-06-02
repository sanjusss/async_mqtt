// Copyright Takatoshi Kondo 2022
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#if !defined(ASYNC_MQTT_PACKET_STORE_PACKET_VARIANT_HPP)
#define ASYNC_MQTT_PACKET_STORE_PACKET_VARIANT_HPP

#include <async_mqtt/util/variant.hpp>
#include <async_mqtt/packet/packet_id_type.hpp>
#include <async_mqtt/packet/v3_1_1_publish.hpp>
#include <async_mqtt/packet/v3_1_1_pubrel.hpp>
#include <async_mqtt/packet/v5_publish.hpp>
#include <async_mqtt/packet/v5_pubrel.hpp>
#include <async_mqtt/exception.hpp>

namespace async_mqtt {

/**
 * @brief corresponding response packet
 */
enum class response_packet {
    v3_1_1_puback,
    v3_1_1_pubrec,
    v3_1_1_pubcomp,
    v5_puback,
    v5_pubrec,
    v5_pubcomp,
};

/**
 * @brief MQTT packet variant for store
 * @tparam PacketIdBytes MQTT spec is 2. You can use `store_packet_variant` for that.
 */
template <std::size_t PacketIdBytes>
class basic_store_packet_variant {
public:
    using packet_id_t = typename packet_id_type<PacketIdBytes>::type;

    basic_store_packet_variant(v3_1_1::basic_publish_packet<PacketIdBytes> packet)
        :res_{
             [&] {
                 switch (packet.opts().get_qos()) {
                 case qos::at_least_once:
                     return response_packet::v3_1_1_puback;
                 case qos::exactly_once:
                     return response_packet::v3_1_1_pubrec;
                 default:
                     throw make_error(
                         errc::bad_message,
                         "qos::at_most_once cannot store"
                     );
                 }
             }()
         },
         var_{force_move(packet)}
    {}

    basic_store_packet_variant(v3_1_1::basic_pubrel_packet<PacketIdBytes> packet)
        :res_{response_packet::v3_1_1_pubcomp},
         var_{force_move(packet)}
    {}

    basic_store_packet_variant(v5::basic_publish_packet<PacketIdBytes> packet)
        :res_{
             [&] {
                 switch (packet.opts().get_qos()) {
                 case qos::at_least_once:
                     return response_packet::v5_puback;
                 case qos::exactly_once:
                     return response_packet::v5_pubrec;
                 default:
                     throw make_error(
                         errc::bad_message,
                         "qos::at_most_once cannot store"
                     );
                 }
             }()
         },
         var_{force_move(packet)}
    {}

    basic_store_packet_variant(v5::basic_pubrel_packet<PacketIdBytes> packet)
        :res_{response_packet::v5_pubcomp},
         var_{force_move(packet)}
    {}

    template <typename Func>
    auto visit(Func&& func) const& {
        return
            async_mqtt::visit(
                std::forward<Func>(func),
                var_
            );
    }

    template <typename Func>
    auto visit(Func&& func) & {
        return
            async_mqtt::visit(
                std::forward<Func>(func),
                var_
            );
    }

    template <typename Func>
    auto visit(Func&& func) && {
        return
            async_mqtt::visit(
                std::forward<Func>(func),
                force_move(var_)
            );
    }

    std::vector<as::const_buffer> const_buffer_sequence() const {
        return visit(
            overload {
                [] (auto const& p) {
                    return p.const_buffer_sequence();
                },
                [] (system_error const&) {
                    BOOST_ASSERT(false);
                    return std::vector<as::const_buffer>{};
                }
            }
        );
    }

    packet_id_t packet_id() const {
        return visit(
            overload {
                [] (auto const& p) {
                    return p.packet_id();
                }
            }
        );
    }

    std::size_t size() const {
        return visit(
            overload {
                [] (auto const& p) -> std::size_t {
                    return p.size();
                }
            }
        );
    }

    std::uint32_t get_message_expiry_interval() const {
        return visit(
            overload {
                [] (v5::basic_publish_packet<PacketIdBytes> const& p) {
                    std::uint32_t ret = 0;
                    bool finish = false;
                    for (auto const& prop : p.props()) {
                        prop.visit(
                            overload {
                                [&](property::message_expiry_interval const& p) {
                                    ret = p.val();
                                    finish = true;
                                },
                                [](auto const&) {
                                }
                            }
                        );
                        if (finish) break;
                    }
                    return ret;
                },
                [] (auto const&) {
                    return std::uint32_t(0);
                }
            }
        );
    }

    void update_message_expiry_interval(std::uint32_t val) const {
        visit(
            overload {
                [&] (v5::basic_publish_packet<PacketIdBytes>& p) {
                    bool finish = false;
                    for (auto& prop : p.props()) {
                        prop.visit(
                            overload {
                                [&](property::message_expiry_interval& p) {
                                    p = property::message_expiry_interval{val};
                                    finish = true;
                                },
                                [](auto&) {
                                }
                            }
                        );
                        if (finish) break;
                    }
                },
                [] (auto&) {
                }
            }
        );
    }

    response_packet response_packet_type() const {
        return res_;
    }

private:
    using variant_t = variant<
        v3_1_1::basic_publish_packet<PacketIdBytes>,
        v3_1_1::basic_pubrel_packet<PacketIdBytes>,
        v5::basic_publish_packet<PacketIdBytes>,
        v5::basic_pubrel_packet<PacketIdBytes>
    >;

    response_packet res_;
    variant_t var_;
};

template <std::size_t PacketIdBytes>
inline std::ostream& operator<<(std::ostream& o, basic_store_packet_variant<PacketIdBytes> const& v) {
    v.visit(
        overload {
            [&] (auto const& p) {
                o << p;
            }
        }
    );
    return o;
}

using store_packet_variant = basic_store_packet_variant<2>;

} // namespace async_mqtt

#endif // ASYNC_MQTT_PACKET_STORE_PACKET_VARIANT_HPP
