#include "IdempotentTools.h"

#include <cctype>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#include <openssl/evp.h>


namespace puntodeventa::product {


bool isValidUuid(
        const std::string& uuid
        ) {

    /*
     * Formato esperado:
     *
     * xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
     *
     * Total:
     * 36 caracteres
     */
    if (uuid.size() != 36) {
        return false;
    }


    for (
            std::size_t i = 0;
            i < uuid.size();
            ++i
        ) {

        /*
         * Posiciones de los guiones.
         */
        if (
            i == 8  ||
            i == 13 ||
            i == 18 ||
            i == 23
        ) {

            if (uuid[i] != '-') {
                return false;
            }

            continue;
        }


        /*
         * Todos los demás caracteres
         * deben ser hexadecimales.
         */
        if (
            !std::isxdigit(
                static_cast<unsigned char>(
                    uuid[i]
                )
            )
        ) {
            return false;
        }
    }


    return true;
}



std::string buildCreateProductRequestHash(
        const ::puntodeventa::v1::CreateProductRequest& request
        ) {

    /*
     * Crear contexto SHA-256.
     */
    EVP_MD_CTX* rawContext =
        EVP_MD_CTX_new();


    if (rawContext == nullptr) {

        throw std::runtime_error(
            "No se pudo crear SHA256 context"
        );
    }


    /*
     * Usamos unique_ptr para garantizar que
     * EVP_MD_CTX_free se ejecute incluso si
     * ocurre una excepción.
     */
    const std::unique_ptr<
        EVP_MD_CTX,
        decltype(&EVP_MD_CTX_free)
    > context{
        rawContext,
        EVP_MD_CTX_free
    };


    /*
     * Inicializar SHA-256.
     */
    if (
        EVP_DigestInit_ex(
            context.get(),
            EVP_sha256(),
            nullptr
        ) != 1
    ) {

        throw std::runtime_error(
            "No se pudo inicializar SHA256"
        );
    }


    /*
     * Helper para agregar bytes al hash.
     */
    auto update =
        [&](std::string_view value) {

            if (
                EVP_DigestUpdate(
                    context.get(),
                    value.data(),
                    value.size()
                ) != 1
            ) {

                throw std::runtime_error(
                    "Error calculando SHA256"
                );
            }
        };


    /*
     * Agregamos:
     *
     * nombre:length:value|
     *
     * En vez de concatenar simplemente los valores.
     *
     * Esto evita ambigüedades.
     *
     * Ejemplo:
     *
     * "ab" + "c"
     *
     * contra:
     *
     * "a" + "bc"
     */
    auto addField =
        [&](std::string_view name,
            std::string_view value) {

            update(name);

            update(":");


            const std::string length =
                std::to_string(
                    value.size()
                );


            update(length);

            update(":");

            update(value);

            update("|");
        };


    /*
     * Importante:
     *
     * NO agregamos UUID.
     *
     * UUID identifica la operación.
     *
     * El hash identifica el contenido
     * de la petición.
     */

    addField(
        "nombre",
        request.nombre()
    );


    addField(
        "precio",
        std::to_string(
            request.precio()
        )
    );


    addField(
        "costo",
        std::to_string(
            request.costo()
        )
    );


    addField(
        "descripcion",
        request.descripcion()
    );


    addField(
        "cantidad",
        std::to_string(
            request.cantidad()
        )
    );


    /*
     * ProductImage.data es protobuf bytes,
     * pero en C++ se representa mediante
     * std::string.
     *
     * std::string puede contener bytes binarios,
     * incluyendo '\0'.
     */
    addField(
        "imagen",
        request.imagen().data()
    );


    /*
     * Finalizar SHA-256.
     */
    unsigned char digest[
        EVP_MAX_MD_SIZE
    ];

    unsigned int digestLength = 0;


    if (
        EVP_DigestFinal_ex(
            context.get(),
            digest,
            &digestLength
        ) != 1
    ) {

        throw std::runtime_error(
            "No se pudo finalizar SHA256"
        );
    }


    /*
     * Convertimos el hash binario
     * a hexadecimal.
     */
    std::ostringstream result;


    result
        << std::hex
        << std::setfill('0');


    for (
            unsigned int i = 0;
            i < digestLength;
            ++i
        ) {

        result
            << std::setw(2)
            << static_cast<int>(
                digest[i]
            );
    }


    return result.str();
}


}
