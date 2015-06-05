#include <factory/factory.hh>

using namespace factory;

static_assert(byte_swap<uint16_t>(UINT16_C(0xABCD)) == UINT16_C(0xCDAB), "Byte swap failed for u16");
static_assert(byte_swap<uint32_t>(UINT32_C(0xABCDDCBA)) == UINT32_C(0xBADCCDAB), "Byte swap failed for u32");
static_assert(byte_swap<uint64_t>(UINT64_C(0xABCDDCBA12344321)) == UINT64_C(0x21433412BADCCDAB), "Byte swap failed for u64");

int main(int argc, char* argv[]) {
	union Endian {
		constexpr Endian() {}
		uint32_t i = UINT32_C(1);
		uint8_t b[4];
	};
	Endian endian;
	if ((is_network_byte_order() && endian.b[0] != 0)
		|| (!is_network_byte_order() && endian.b[0] != 1))
	{
		throw std::runtime_error("Endian was not correctly determined at compile time.");
	}
//	uint64_t* y = new uint64_t;
//	uint64_t x = byte_swap<uint64_t>(*y);
//	uint64_t* z = new uint64_t;
//	*z = x;
	return 0;
}
