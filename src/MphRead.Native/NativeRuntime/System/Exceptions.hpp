#pragma once

// The System exception types the game throws or catches, with the messages the
// .NET runtime gives them.

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace System
{
    class ArgumentException final : public std::invalid_argument
    {
    public:
        ArgumentException()
            : std::invalid_argument("Value does not fall within the expected range.")
        {
        }

        explicit ArgumentException(std::string_view message)
            : std::invalid_argument(std::string(message))
        {
        }
    };

    class ArgumentOutOfRangeException final : public std::out_of_range
    {
    public:
        ArgumentOutOfRangeException()
            : std::out_of_range(
                "Index was out of range. Must be non-negative and less than the size "
                "of the collection. (Parameter 'index')")
        {
        }

        explicit ArgumentOutOfRangeException(std::string_view paramName)
            : std::out_of_range("Specified argument was out of the range of valid values. (Parameter '"
                + std::string(paramName) + "')")
        {
        }
    };

    class ArgumentNullException final : public std::invalid_argument
    {
    public:
        explicit ArgumentNullException(std::string_view paramName)
            : std::invalid_argument(
                "Value cannot be null. (Parameter '" + std::string(paramName) + "')")
        {
        }
    };

    class FormatException final : public std::invalid_argument
    {
    public:
        FormatException()
            : std::invalid_argument("Input string was not in a correct format.")
        {
        }

        explicit FormatException(std::string_view message)
            : std::invalid_argument(std::string(message))
        {
        }
    };

    class OverflowException final : public std::overflow_error
    {
    public:
        OverflowException()
            : std::overflow_error("Arithmetic operation resulted in an overflow.")
        {
        }

        explicit OverflowException(std::string_view message)
            : std::overflow_error(std::string(message))
        {
        }
    };

    class UnauthorizedAccessException final : public std::runtime_error
    {
    public:
        explicit UnauthorizedAccessException(std::string message)
            : std::runtime_error(std::move(message))
        {
        }
    };

    namespace IO
    {
        class IOException : public std::runtime_error
        {
        public:
            explicit IOException(std::string message)
                : std::runtime_error(std::move(message))
            {
            }
        };

        class FileNotFoundException final : public IOException
        {
        public:
            explicit FileNotFoundException(std::string message)
                : IOException(std::move(message))
            {
            }
        };

        class DirectoryNotFoundException final : public IOException
        {
        public:
            explicit DirectoryNotFoundException(std::string message)
                : IOException(std::move(message))
            {
            }
        };

        class PathTooLongException final : public IOException
        {
        public:
            explicit PathTooLongException(std::string message)
                : IOException(std::move(message))
            {
            }
        };

        class InvalidDataException final : public std::runtime_error
        {
        public:
            InvalidDataException()
                : std::runtime_error("Found invalid data while decoding.")
            {
            }

            explicit InvalidDataException(std::string_view message)
                : std::runtime_error(std::string(message))
            {
            }
        };

        class EndOfStreamException final : public IOException
        {
        public:
            EndOfStreamException()
                : IOException("Unable to read beyond the end of the stream.")
            {
            }
        };
    }

    class NotImplementedException final : public std::logic_error
    {
    public:
        NotImplementedException()
            : std::logic_error("The method or operation is not implemented.")
        {
        }
    };

    class NullReferenceException final : public std::runtime_error
    {
    public:
        NullReferenceException()
            : std::runtime_error("Object reference not set to an instance of an object.")
        {
        }
    };

    class InvalidOperationException final : public std::logic_error
    {
    public:
        InvalidOperationException()
            : std::logic_error("Operation is not valid due to the current state of the object.")
        {
        }

        explicit InvalidOperationException(std::string_view message)
            : std::logic_error(std::string(message))
        {
        }
    };

    class InvalidCastException final : public std::runtime_error
    {
    public:
        InvalidCastException()
            : std::runtime_error("Specified cast is not valid.")
        {
        }
    };

    class IndexOutOfRangeException final : public std::out_of_range
    {
    public:
        IndexOutOfRangeException()
            : std::out_of_range("Index was outside the bounds of the array.")
        {
        }
    };

    class DivideByZeroException final : public std::runtime_error
    {
    public:
        DivideByZeroException()
            : std::runtime_error("Attempted to divide by zero.")
        {
        }
    };

    class NotSupportedException final : public std::logic_error
    {
    public:
        NotSupportedException()
            : std::logic_error("Specified method is not supported.")
        {
        }

        explicit NotSupportedException(std::string_view message)
            : std::logic_error(std::string(message))
        {
        }
    };

    class ObjectDisposedException final : public std::runtime_error
    {
    public:
        explicit ObjectDisposedException(std::string_view objectName)
            : std::runtime_error("Cannot access a closed " + std::string(objectName) + ".")
        {
        }
    };

    class OperationCanceledException final : public std::runtime_error
    {
    public:
        OperationCanceledException()
            : std::runtime_error("The operation was canceled.")
        {
        }
    };

    class OutOfMemoryException final : public std::runtime_error
    {
    public:
        OutOfMemoryException()
            : std::runtime_error(
                "Insufficient memory to continue the execution of the program.")
        {
        }
    };
}
