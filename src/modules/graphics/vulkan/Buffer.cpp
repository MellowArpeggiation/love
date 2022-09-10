#include "Buffer.h"
#include "Graphics.h"

namespace love
{
namespace graphics
{
namespace vulkan
{

static VkBufferUsageFlags getUsageBit(BufferUsage mode)
{
	switch (mode)
	{
	case BUFFERUSAGE_VERTEX: return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	case BUFFERUSAGE_INDEX: return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	case BUFFERUSAGE_UNIFORM: return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	default:
		throw love::Exception("unsupported BufferUsage mode");
	}
}

static VkBufferUsageFlags getVulkanUsageFlags(BufferUsageFlags flags)
{
	VkBufferUsageFlags vkFlags = 0;
	for (int i = 0; i < BUFFERUSAGE_MAX_ENUM; i++)
	{
		BufferUsageFlags flag = static_cast<BufferUsageFlags>(1u << i);
		if (flags & flag)
			vkFlags |= getUsageBit((BufferUsage)i);
	}
	return vkFlags;
}

Buffer::Buffer(love::graphics::Graphics *gfx, const Settings &settings, const std::vector<DataDeclaration> &format, const void *data, size_t size, size_t arraylength)
	: love::graphics::Buffer(gfx, settings, format, size, arraylength)
	, usageFlags(settings.usageFlags)
	, vgfx(dynamic_cast<Graphics*>(gfx)) 
{
	loadVolatile();
}

bool Buffer::loadVolatile()
{
	allocator = vgfx->getVmaAllocator();

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = getSize();
	if (dataUsage == BUFFERDATAUSAGE_READBACK)
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	else
		bufferInfo.usage = getVulkanUsageFlags(usageFlags);

	VmaAllocationCreateInfo allocCreateInfo = {};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

	vmaCreateBuffer(allocator, &bufferInfo, &allocCreateInfo, &buffer, &allocation, &allocInfo);

	return true;
}

void Buffer::unloadVolatile()
{
	if (buffer == VK_NULL_HANDLE)
		return;

	auto device = vgfx->getDevice();

	vgfx->queueCleanUp(
		[device=device, allocator=allocator, buffer=buffer, allocation=allocation](){
		vkDeviceWaitIdle(device);
		vmaDestroyBuffer(allocator, buffer, allocation);
	});

	buffer = VK_NULL_HANDLE;
}

Buffer::~Buffer()
{
	unloadVolatile();
}

ptrdiff_t Buffer::getHandle() const
{
	return (ptrdiff_t) buffer;
}

ptrdiff_t Buffer::getTexelBufferHandle() const
{
	throw love::Exception("unimplemented Buffer::getTexelBufferHandle");
	return (ptrdiff_t) nullptr;	// todo ?
}

void* Buffer::map(MapType map, size_t offset, size_t size)
{
	char* data = (char*)allocInfo.pMappedData;
	return (void*) (data + offset);
}

bool Buffer::fill(size_t offset, size_t size, const void *data)
{
	void* dst = (void*)((char*)allocInfo.pMappedData + offset);
	memcpy(dst, data, size);
	return true;
}

void Buffer::unmap(size_t usedoffset, size_t usedsize)
{
	(void)usedoffset;
	(void)usedsize;
}

void Buffer::copyTo(love::graphics::Buffer *dest, size_t sourceoffset, size_t destoffset, size_t size)
{
	auto commandBuffer = vgfx->getCommandBufferForDataTransfer();

	VkBufferCopy bufferCopy{};
	bufferCopy.srcOffset = sourceoffset;
	bufferCopy.dstOffset = destoffset;
	bufferCopy.size = size;

	vkCmdCopyBuffer(commandBuffer, buffer, (VkBuffer) dest->getHandle(), 1, &bufferCopy);
}

} // vulkan
} // graphics
} // love
