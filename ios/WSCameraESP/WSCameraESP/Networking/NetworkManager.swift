
import Foundation
import MightFail

enum NetworkError: Error {
  case unexpectedStatusCode(actual: Int?)
  case decodingFailed(Error)
}

final actor NetworkManager {
  let session: URLSession

  init() {
    let configuration = URLSessionConfiguration.default
    session = URLSession(configuration: configuration)
  }
}

extension NetworkManager {
  func sendRequest<T: Decodable>(
    _ request: URLRequest,
    expectedStatusCode: Int
  ) async throws -> T {
    let (data, response) = try await session.data(for: request)
    guard let httpResponse = response as? HTTPURLResponse,
          httpResponse.statusCode == expectedStatusCode
    else {
      throw NetworkError.unexpectedStatusCode(actual: (response as? HTTPURLResponse)?.statusCode)
    }

    let (error, value) = mightFail { try JSONDecoder().decode(T.self, from: data) }
    guard let value else { throw NetworkError.decodingFailed(error) }
    return value
  }

  func sendRequestWithoutBody(
    _ request: URLRequest,
    expectedStatusCode: Int
  ) async throws {
    let (_, response) = try await session.data(for: request)
    guard let httpResponse = response as? HTTPURLResponse,
          httpResponse.statusCode == expectedStatusCode
    else {
      throw NetworkError.unexpectedStatusCode(actual: (response as? HTTPURLResponse)?.statusCode)
    }
  }

  func sendDownloadRequest(
    _ request: URLRequest,
    expectedStatusCode: Int
  ) async throws -> URL {
    let (tempLocalURL, response) = try await session.download(for: request)
    guard let httpResponse = response as? HTTPURLResponse,
          httpResponse.statusCode == expectedStatusCode
    else {
      throw NetworkError.unexpectedStatusCode(actual: (response as? HTTPURLResponse)?.statusCode)
    }
    return tempLocalURL
  }
}
