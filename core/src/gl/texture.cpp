#include "texture.h"

#include "platform.h"
#include "util/geom.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <cstring> // for memset

GLuint Texture::s_boundTextures[] = { 0 };
GLuint Texture::s_activeSlot = GL_TEXTURE0;
int Texture::s_validGeneration = 0;

Texture::Texture(unsigned int _width, unsigned int _height, TextureOptions _options, bool _generateMipmaps)
    : m_options(_options), m_generateMipmaps(_generateMipmaps) {

    m_glHandle = 0;
    m_dirty = false;
    m_shouldResize = false;
    m_target = GL_TEXTURE_2D;
    m_generation = -1;

    resize(_width, _height);
}

Texture::Texture(const std::string& _file, TextureOptions _options, bool _generateMipmaps)
    : Texture(0, 0, _options, _generateMipmaps) {

    unsigned int size;
    unsigned char* data = bytesFromResource(_file.c_str(), &size);
    unsigned char* pixels;
    int width, height, comp;

    pixels = stbi_load_from_memory(data, size, &width, &height, &comp, STBI_rgb_alpha);

    resize(width, height);
    setData(reinterpret_cast<GLuint*>(pixels), width * height);
    update(0);

    free(data);
    stbi_image_free(pixels);
}

Texture::~Texture() {
    for (size_t i = 0; i < GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS; i++) {
        if (s_boundTextures[i] == m_glHandle) { s_boundTextures[i] = 0; }
    }
    if (m_glHandle) { glDeleteTextures(1, &m_glHandle); }
}

void Texture::bind(GLuint _textureSlot) {

    if (_textureSlot >= GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS) {

        // Trying to access an invalid texture unit
        return;
    }

    if (_textureSlot != s_activeSlot) {

        glActiveTexture(getTextureUnit(_textureSlot));
        s_activeSlot = _textureSlot;
    }

    if (s_boundTextures[_textureSlot] != m_glHandle) {

        glBindTexture(m_target, m_glHandle);
        s_boundTextures[_textureSlot] = m_glHandle;
    }
}

void Texture::setData(const GLuint* _data, unsigned int _dataSize) {

    if (m_data.size() > 0) { m_data.clear(); }

    m_data.insert(m_data.begin(), _data, _data + _dataSize);

    m_dirty = true;
}

void Texture::setSubData(const GLuint* _subData, unsigned int _xoff, unsigned int _yoff, unsigned int _width,
                         unsigned int _height) {

    std::unique_ptr<std::vector<GLuint>> subData(new std::vector<GLuint>(_subData, _subData + (_width * _height)));

    // update m_data with subdata
    size_t bpp = bytesPerPixel();
    size_t divisor = sizeof(GLuint) / bpp;
    for (size_t j = 0; j < _height; j++) {
        size_t dpos = ((j + _yoff) * m_width + _xoff) / divisor;
        size_t spos = (j * _width) / divisor;
        std::memcpy(&m_data[dpos], &_subData[spos], _width * bpp);
    }

    m_subData.push(std::unique_ptr<TextureSubData>(new TextureSubData
        {std::move(subData), _xoff, _yoff, _width, _height}
    ));

    m_dirty = true;
}

void Texture::generate(GLuint _textureUnit) {
    glGenTextures(1, &m_glHandle);

    bind(_textureUnit);

    if (m_generateMipmaps) {
        GLenum mipmapFlags = GL_LINEAR_MIPMAP_LINEAR | GL_LINEAR_MIPMAP_NEAREST | GL_NEAREST_MIPMAP_LINEAR | GL_NEAREST_MIPMAP_NEAREST;
        if (m_options.m_filtering.m_min & mipmapFlags) {
            logMsg("Warning: wrong options provided for the usage of mipmap generation\n");
        }
    }

    glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, m_options.m_filtering.m_min);
    glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, m_options.m_filtering.m_mag);

    glTexParameteri(m_target, GL_TEXTURE_WRAP_S, m_options.m_wrapping.m_wraps);
    glTexParameteri(m_target, GL_TEXTURE_WRAP_T, m_options.m_wrapping.m_wrapt);

    m_generation = s_validGeneration;
}

void Texture::checkValidity() {

    if (m_generation != s_validGeneration) {
        m_dirty = true;
        m_shouldResize = true;
        m_glHandle = 0;
    }
}

void Texture::update(GLuint _textureUnit) {

    checkValidity();

    if (!m_dirty) { return; }

    if (m_glHandle == 0) { // texture hasn't been initialized yet, generate it

        generate(_textureUnit);

        if (m_data.size() == 0) { m_data.assign(m_width * m_height, 0); }

    } else { bind(_textureUnit); }

    GLuint* data = m_data.size() > 0 ? m_data.data() : nullptr;

    // resize or push data
    if (m_shouldResize) {
        glTexImage2D(m_target, 0, m_options.m_internalFormat, m_width, m_height, 0, m_options.m_format, GL_UNSIGNED_BYTE, data);

        if (data && m_generateMipmaps) {
            // generate the mipmaps for this texture
            glGenerateMipmap(m_target);
        }

        m_shouldResize = false;
    }

    // process queued sub data updates
    while (m_subData.size() > 0) {
        const TextureSubData* subData = m_subData.front().get();

        glTexSubImage2D(m_target, 0, subData->m_xoff, subData->m_yoff, subData->m_width, subData->m_height,
                        m_options.m_format, GL_UNSIGNED_BYTE, subData->m_data->data());

        m_subData.pop();
    }

    m_dirty = false;
}

void Texture::resize(const unsigned int _width, const unsigned int _height) {
    m_width = _width;
    m_height = _height;

    m_shouldResize = true;
    m_dirty = true;
}

size_t Texture::bytesPerPixel() {
    switch (m_options.m_internalFormat) {
        case GL_ALPHA:
        case GL_LUMINANCE:
            return 1;
        case GL_LUMINANCE_ALPHA:
            return 2;
        case GL_RGB:
            return 3;
        default:
            return 4;
    }
}

GLuint Texture::getTextureUnit(GLuint _unit) {
    if (_unit >= GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS) { logMsg("Warning: trying to access unavailable texture unit"); }

    return GL_TEXTURE0 + _unit;
}

void Texture::invalidateAllTextures() {

    for (GLuint i = 0; i < GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS; ++i) {
        s_boundTextures[i] = 0;
    }
    s_activeSlot = GL_TEXTURE0;

    ++s_validGeneration;

}